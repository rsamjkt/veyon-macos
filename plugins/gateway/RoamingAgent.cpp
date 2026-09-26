/*
 * RoamingAgent.cpp - keeps a roaming laptop reachable through the office gateway
 *
 * Copyright (c) 2026 AruniControl Community
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QDir>
#include <QFileInfo>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QSysInfo>

#include "AccessControlRule.h"
#include "PlatformFilesystemFunctions.h"
#include "PlatformUserFunctions.h"
#include "RoamingAgent.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


namespace {

constexpr auto StatusFile = "roaming-status.json";

// close code of the relay when the gateway is not connected
constexpr int CloseGatewayOffline = 4404;

}



RoamingAgent::RoamingAgent( QObject* parent ) :
	TunnelEndpoint( parent ),
	m_instanceLock( QDir( VeyonCore::platform().filesystemFunctions().globalTempPath() ).filePath( QStringLiteral("aruni-roaming.lock") ) )
{
	m_instanceLock.setStaleLockTime( 0 );

	connect( &m_socket, &QWebSocket::connected, this, &RoamingAgent::onConnected );
	connect( &m_socket, &QWebSocket::binaryMessageReceived, this, &RoamingAgent::onBinaryMessage );
	connect( &m_socket, &QWebSocket::disconnected, this, &RoamingAgent::onDisconnected );
	connect( &m_socket, &QWebSocket::errorOccurred, this, [this]( QAbstractSocket::SocketError ) {
		m_attemptError = m_socket.errorString();
	} );

	m_reconnectTimer.setSingleShot( true );
	connect( &m_reconnectTimer, &QTimer::timeout, this, &RoamingAgent::connectToGateway );

	connect( &m_stateTimer, &QTimer::timeout, this, &RoamingAgent::checkState );
	m_stateTimer.start( StateCheckInterval );

	// keeps NAT mappings and proxies from dropping an idle session
	connect( &m_pingTimer, &QTimer::timeout, this, [this]() {
		sendFrame( AruniTunnel::FrameType::Ping, 0 );
	} );
	// addresses change when the laptop moves between networks
	connect( &m_helloTimer, &QTimer::timeout, this, &RoamingAgent::sendHello );

	m_handshakeTimer.setSingleShot( true );
	connect( &m_handshakeTimer, &QTimer::timeout, this, [this]() {
		if( m_established == false )
		{
			m_attemptError = tr( "The office gateway did not answer" );
			m_socket.abort();
		}
	} );

	checkState();
}



RoamingAgent::~RoamingAgent()
{
	m_socket.close();
	if( m_haveLock )
	{
		GatewayState::writeStatus( { { QStringLiteral("running"), false } }, QLatin1String( StatusFile ) );
		m_instanceLock.unlock();
	}
}



QJsonObject RoamingAgent::helloInfo()
{
	QJsonArray addresses;
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		if( iface.flags().testFlag( QNetworkInterface::IsUp ) == false ||
			iface.flags().testFlag( QNetworkInterface::IsLoopBack ) )
		{
			continue;
		}
		for( const auto& entry : iface.addressEntries() )
		{
			if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol )
			{
				addresses.append( entry.ip().toString() );
			}
		}
	}

	return QJsonObject{
		{ QStringLiteral("name"), QHostInfo::localHostName() },
		{ QStringLiteral("os"), QSysInfo::prettyProductName() },
		{ QStringLiteral("version"), VeyonCore::versionString() },
		{ QStringLiteral("user"), VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName ) },
		{ QStringLiteral("addresses"), addresses },
	};
}



QString RoamingAgent::forwardingBlockedReason()
{
	// forwarded connections reach the server from 127.0.0.1 - settings that
	// trust local connections would let anyone reaching the gateway in
	if( VeyonCore::config().localConnectOnly() )
	{
		return tr( "Not possible while \"Allow connections from localhost only\" is enabled" );
	}

	if( VeyonCore::config().isAccessControlRulesProcessingEnabled() )
	{
		const auto rules = VeyonCore::config().accessControlRules();
		for( const auto& value : rules )
		{
			if( AccessControlRule( value ).isConditionEnabled( AccessControlRule::Condition::AccessFromLocalHost ) )
			{
				return tr( "Not possible with access control rules for connections from the local computer" );
			}
		}
	}

	return {};
}



bool RoamingAgent::isAllowedTarget( const QHostAddress& address, quint16 port ) const
{
	// the gateway may only reach this computer's own AruniControl server
	const auto basePort = VeyonCore::config().veyonServerPort();
	return address.isLoopback() && port >= basePort && port < basePort + 100;
}



void RoamingAgent::checkState()
{
	const auto modified = QFileInfo( GatewayState::statePath() ).lastModified();
	if( modified != m_stateModified )
	{
		const auto previous = m_state;
		m_state = GatewayState::load();
		m_stateModified = modified;

		const bool hubChanged = previous.roamingHub.gatewayId != m_state.roamingHub.gatewayId ||
								previous.roamingHub.relayUrl != m_state.roamingHub.relayUrl ||
								previous.roamingHub.token != m_state.roamingHub.token;
		if( hubChanged )
		{
			m_rejected = false;
			m_refusals = 0;
			m_lastError.clear();
			m_reconnectDelay = 2000;
			if( m_socket.state() != QAbstractSocket::UnconnectedState )
			{
				m_socket.close();
			}
			else if( m_reconnectTimer.isActive() )
			{
				m_reconnectTimer.start( 0 );
			}
		}
	}

	m_blockedReason = forwardingBlockedReason();
	const bool wanted = m_state.roamingEnabled && m_state.roamingHub.gatewayId.isEmpty() == false &&
						m_state.roamingHub.gatewayPublicKey.size() == AruniTunnel::KeySize &&
						m_blockedReason.isEmpty();

	if( wanted == false )
	{
		if( m_haveLock )
		{
			m_reconnectTimer.stop();
			m_socket.close();
			m_instanceLock.unlock();
			m_haveLock = false;
		}
		// the laptop is not connected either way - report why (once per change)
		const auto reason = m_state.roamingEnabled ? m_blockedReason : QString{};
		if( m_haveLock == false && reason != m_reportedIdleReason )
		{
			m_reportedIdleReason = reason;
			GatewayState::writeStatus( { { QStringLiteral("running"), reason.isEmpty() == false },
										 { QStringLiteral("enabled"), m_state.roamingEnabled },
										 { QStringLiteral("error"), reason } }, QLatin1String( StatusFile ) );
		}
		return;
	}

	m_reportedIdleReason = QStringLiteral("-");

	// only one server instance per computer keeps the session
	if( m_haveLock == false )
	{
		m_haveLock = m_instanceLock.tryLock( 0 );
		if( m_haveLock == false )
		{
			return;
		}
		// persist the laptop's key so the Configurator and a restart use it
		GatewayState::update( []( GatewayState& ) {} );
		m_state = GatewayState::load();
		m_stateModified = QFileInfo( GatewayState::statePath() ).lastModified();
	}

	if( m_socket.state() == QAbstractSocket::UnconnectedState && m_reconnectTimer.isActive() == false )
	{
		connectToGateway();
	}

	writeStatus();
}



void RoamingAgent::connectToGateway()
{
	if( m_haveLock == false || m_state.roamingEnabled == false )
	{
		return;
	}

	QUrl url( m_state.roamingHub.relayUrl );
	url.setPath( url.path() + QStringLiteral("/v1/connect/") + m_state.roamingHub.gatewayId );

	m_established = false;
	m_relayConnected = false;
	m_attemptError.clear();
	// nothing of the previous connection may leak into this one
	resetStreams();
	m_socket.open( url );
	m_handshakeTimer.start( HandshakeTimeout );
}



void RoamingAgent::onConnected()
{
	m_relayConnected = true;

	// the enrollment token is only presented until the gateway accepted the
	// laptop once - a laptop removed by the admin must not re-enroll itself
	m_handshake = std::make_unique<AruniTunnel::MasterHandshake>(
		m_state.roamingKeyPair, m_state.roamingHub.gatewayPublicKey,
		m_state.roamingRegistered ? QByteArray{} : m_state.roamingHub.token,
		QHostInfo::localHostName(), true );
	m_socket.sendBinaryMessage( m_handshake->firstMessage() );
}



void RoamingAgent::onBinaryMessage( const QByteArray& message )
{
	if( m_established == false )
	{
		QByteArray gatewayInfo;
		if( m_handshake == nullptr || m_handshake->processReply( message, m_channel, gatewayInfo ) == false )
		{
			m_attemptError = tr( "Invalid answer from the office gateway" );
			m_socket.abort();
			return;
		}

		m_handshake.reset();
		m_handshakeTimer.stop();
		m_established = true;
		m_hubAddresses.clear();
		for( const auto& address : QJsonDocument::fromJson( gatewayInfo )[QStringLiteral("addresses")].toArray() )
		{
			m_hubAddresses.append( address.toString() );
		}
		m_rejected = false;
		m_refusals = 0;
		m_lastError.clear();
		m_reconnectDelay = 2000;
		m_connectedSince = QDateTime::currentDateTimeUtc();

		if( m_state.roamingRegistered == false )
		{
			// only for the gateway this session belongs to - the user may have
			// entered another code meanwhile; the next state check reloads
			const auto hub = m_state.roamingHub;
			GatewayState::update( [&hub]( GatewayState& state ) {
				if( state.roamingHub.gatewayId == hub.gatewayId && state.roamingHub.token == hub.token )
				{
					state.roamingRegistered = true;
				}
			} );
			m_state.roamingRegistered = true;
		}

		vInfo() << "roaming laptop connected to office gateway" << m_state.roamingHub.siteName;

		sendHello();
		m_pingTimer.start( PingInterval );
		m_helloTimer.start( HelloInterval );
		writeStatus();
		return;
	}

	QByteArray frame;
	if( m_channel.decrypt( message, frame ) == false )
	{
		vWarning() << "roaming laptop: invalid message from the office gateway - reconnecting";
		m_socket.abort();
		return;
	}

	AruniTunnel::FrameType type;
	quint32 stream = 0;
	QByteArray payload;
	if( AruniTunnel::parseFrame( frame, type, stream, payload ) == false )
	{
		return;
	}

	if( handleStreamFrame( type, stream, payload ) )
	{
		return;
	}

	switch( type )
	{
	case AruniTunnel::FrameType::Open:
		openStream( stream, payload );
		break;
	case AruniTunnel::FrameType::Ping:
		sendFrame( AruniTunnel::FrameType::Pong, stream, payload );
		break;
	default:
		break;
	}
}



void RoamingAgent::onDisconnected()
{
	const bool wasEstablished = m_established;
	const auto closeCode = int( m_socket.closeCode() );

	m_established = false;
	m_handshake.reset();
	m_handshakeTimer.stop();
	m_pingTimer.stop();
	m_helloTimer.stop();
	closeAllStreams();

	if( wasEstablished )
	{
		m_lastError = m_attemptError.isEmpty() ? tr( "Connection to the office gateway lost" ) : m_attemptError;
		vWarning() << "roaming laptop lost the connection to the office gateway:" << m_lastError;
	}
	else if( closeCode == CloseGatewayOffline )
	{
		m_lastError = tr( "The office gateway is offline" );
		m_refusals = 0;
	}
	else if( m_relayConnected && m_attemptError.isEmpty() )
	{
		// the gateway ended the session right after our handshake - twice in a
		// row means it does not know (any more) this laptop
		m_rejected = ++m_refusals >= 2;
		m_lastError = tr( "The office gateway refused this laptop (removed, or the enrollment code was renewed)" );
	}
	else
	{
		m_lastError = m_attemptError.isEmpty() ? tr( "Cannot reach the relay server" ) : m_attemptError;
	}

	writeStatus();

	if( m_haveLock && m_state.roamingEnabled )
	{
		m_reconnectTimer.start( m_rejected ? RejectedRetryDelay : m_reconnectDelay );
		m_reconnectDelay = qMin( m_reconnectDelay * 2, MaxReconnectDelay );
	}
}



void RoamingAgent::sendHello()
{
	probeOffice( [this]( bool inOffice ) {
		m_inOffice = inOffice;
		auto hello = helloInfo();
		hello[QStringLiteral("local")] = inOffice;
		sendFrame( AruniTunnel::FrameType::AgentHello, 0, QJsonDocument( hello ).toJson( QJsonDocument::Compact ) );
		writeStatus();
	} );
}



void RoamingAgent::probeOffice( const std::function<void(bool)>& done )
{
	// for testing the away case on the gateway's own network
	if( m_hubAddresses.isEmpty() || qEnvironmentVariableIsSet( "ARUNI_ROAMING_FORCE_AWAY" ) )
	{
		done( false );
		return;
	}

	struct Probe
	{
		int pending{0};
		bool found{false};
		bool reported{false};
	};
	auto probe = std::make_shared<Probe>();
	probe->pending = int( m_hubAddresses.size() );
	const auto hubId = m_state.roamingHub.gatewayId;

	const auto finishOne = [probe, done]( bool found ) {
		probe->found = probe->found || found;
		--probe->pending;
		if( probe->reported == false && ( probe->found || probe->pending <= 0 ) )
		{
			probe->reported = true;
			done( probe->found );
		}
	};

	for( const auto& address : std::as_const( m_hubAddresses ) )
	{
		auto socket = new QTcpSocket( this );
		auto buffer = std::make_shared<QByteArray>();
		auto finished = std::make_shared<bool>( false );
		const auto complete = [socket, buffer, finished, finishOne, hubId]( bool answered ) {
			if( *finished )
			{
				return;
			}
			*finished = true;
			socket->abort();
			socket->deleteLater();
			finishOne( answered && QJsonDocument::fromJson( *buffer )[QStringLiteral("id")].toString() == hubId );
		};

		connect( socket, &QTcpSocket::readyRead, socket, [socket, buffer]() { buffer->append( socket->readAll() ); } );
		connect( socket, &QTcpSocket::disconnected, socket, [complete]() { complete( true ); } );
		connect( socket, &QTcpSocket::errorOccurred, socket, [complete]( QAbstractSocket::SocketError error ) {
			complete( error == QAbstractSocket::RemoteHostClosedError );
		} );
		QTimer::singleShot( OfficeProbeTimeout, socket, [complete]() { complete( false ); } );
		socket->connectToHost( address, GatewayState::directoryPort() );
	}
}



void RoamingAgent::writeStatus()
{
	if( m_haveLock == false )
	{
		return;
	}

	GatewayState::writeStatus( {
		{ QStringLiteral("running"), true },
		{ QStringLiteral("enabled"), m_state.roamingEnabled },
		{ QStringLiteral("connected"), m_established },
		{ QStringLiteral("rejected"), m_rejected },
		{ QStringLiteral("error"), m_lastError },
		{ QStringLiteral("site"), m_state.roamingHub.siteName },
		{ QStringLiteral("inOffice"), m_established && m_inOffice },
		{ QStringLiteral("since"), m_established ? m_connectedSince.toString( Qt::ISODate ) : QString{} },
		{ QStringLiteral("updated"), QDateTime::currentDateTimeUtc().toString( Qt::ISODate ) },
	}, QLatin1String( StatusFile ) );
}
