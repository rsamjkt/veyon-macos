/*
 * GatewayService.cpp - Aruni Gateway running inside veyon-server
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
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QNetworkRequest>
#include <QSet>
#include <QUdpSocket>

#include "Filesystem.h"
#include "GatewayService.h"
#include "GatewaySession.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "PlatformFilesystemFunctions.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


namespace {

// the network discovery plugin also merges the manually configured computers
const auto NetworkDiscoveryPluginUid = Plugin::Uid( QStringLiteral("3c5e9a14-2b7d-4e6f-8a1c-9d0f2e4b6c81") );

}



GatewayService::GatewayService( QObject* parent ) :
	QObject( parent ),
	m_instanceLock( QDir( VeyonCore::platform().filesystemFunctions().globalTempPath() ).filePath( QStringLiteral("aruni-gateway.lock") ) )
{
	m_instanceLock.setStaleLockTime( 0 );

	connect( &m_control, &QWebSocket::connected, this, [this]() {
		m_connected = true;
		m_lastError.clear();
		m_reconnectDelay = 2000;
		vInfo() << "Aruni Gateway" << m_state.gatewayId << "connected to relay" << m_state.relayUrl;
		writeStatus();
	} );
	connect( &m_control, &QWebSocket::textMessageReceived, this, &GatewayService::onTextMessage );
	connect( &m_control, &QWebSocket::disconnected, this, &GatewayService::onDisconnected );
	connect( &m_control, &QWebSocket::errorOccurred, this, [this]( QAbstractSocket::SocketError ) {
		m_lastError = m_control.errorString();
	} );

	m_reconnectTimer.setSingleShot( true );
	connect( &m_reconnectTimer, &QTimer::timeout, this, &GatewayService::connectToRelay );

	connect( &m_stateTimer, &QTimer::timeout, this, &GatewayService::checkState );
	m_stateTimer.start( StateCheckInterval );

	connect( &m_directoryTimer, &QTimer::timeout, this, &GatewayService::updateDirectory );

	checkState();
}



GatewayService::~GatewayService()
{
	m_control.close();
	if( m_haveLock )
	{
		GatewayState::writeStatus( { { QStringLiteral("running"), false } } );
		m_instanceLock.unlock();
	}
}



bool GatewayService::authorizeDevice( const QByteArray& deviceKey, const QByteArray& pairingToken, const QString& deviceName )
{
	for( const auto& device : std::as_const( m_state.devices ) )
	{
		if( device.publicKey == deviceKey )
		{
			return true;
		}
	}

	if( pairingToken.isEmpty() || m_state.isPairingActive() == false || pairingToken != m_state.pairingToken )
	{
		return false;
	}

	// the pairing code is single use: register the device and invalidate it
	GatewayState::Device device;
	device.publicKey = deviceKey;
	device.name = deviceName.isEmpty() ? tr("Phone") : deviceName;
	device.added = QDateTime::currentDateTimeUtc();
	device.lastSeen = device.added;

	const bool saved = GatewayState::update( [&device]( GatewayState& state ) {
		state.devices.append( device );
		state.pairingToken.clear();
		state.pairingExpires = {};
	} );

	if( saved )
	{
		m_state.devices.append( device );
		m_state.pairingToken.clear();
		m_stateModified = QFileInfo( GatewayState::statePath() ).lastModified();
		vInfo() << "Aruni Gateway: paired new device" << device.name;
	}

	return saved;
}



QString GatewayService::deviceName( const QByteArray& deviceKey ) const
{
	for( const auto& device : m_state.devices )
	{
		if( device.publicKey == deviceKey )
		{
			return device.name;
		}
	}
	return {};
}



void GatewayService::markDeviceSeen( const QByteArray& deviceKey )
{
	GatewayState::update( [&deviceKey]( GatewayState& state ) {
		for( auto& device : state.devices )
		{
			if( device.publicKey == deviceKey )
			{
				device.lastSeen = QDateTime::currentDateTimeUtc();
			}
		}
	} );
	m_stateModified = QFileInfo( GatewayState::statePath() ).lastModified();
}



QByteArray GatewayService::gatewayInfo() const
{
	return QJsonDocument( QJsonObject{
		{ QStringLiteral("name"), m_state.siteName },
		{ QStringLiteral("id"), m_state.gatewayId },
		{ QStringLiteral("version"), VeyonCore::versionString() },
		{ QStringLiteral("port"), VeyonCore::config().veyonServerPort() },
		{ QStringLiteral("subnets"), QJsonArray::fromStringList( localSubnets() ) },
		{ QStringLiteral("keyAvailable"), sharedKeyJson().isEmpty() == false },
	} ).toJson( QJsonDocument::Compact );
}



QByteArray GatewayService::sharedKeyJson() const
{
	if( m_state.sharedKeyName.isEmpty() || VeyonCore::isAuthenticationKeyNameValid( m_state.sharedKeyName ) == false )
	{
		return {};
	}

	QFile file( VeyonCore::filesystem().privateKeyPath( m_state.sharedKeyName ) );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}

	return QJsonDocument( QJsonObject{
		{ QStringLiteral("name"), m_state.sharedKeyName },
		{ QStringLiteral("pem"), QString::fromUtf8( file.readAll() ) },
	} ).toJson( QJsonDocument::Compact );
}



QByteArray GatewayService::hostsJson() const
{
	QJsonArray hosts;
	QSet<QString> seen;

	if( m_directory )
	{
		const auto objects = m_directory->queryObjects( NetworkObject::Type::Host, NetworkObject::Attribute::None, {} );
		for( const auto& object : objects )
		{
			const auto host = object.hostAddress().isEmpty() ? object.name() : object.hostAddress();
			if( host.isEmpty() || seen.contains( host ) )
			{
				continue;
			}
			seen.insert( host );

			QString location;
			const auto parents = m_directory->queryParents( object );
			for( const auto& parent : parents )
			{
				if( parent.type() == NetworkObject::Type::Location )
				{
					location = parent.name();
					break;
				}
			}

			hosts.append( QJsonObject{
				{ QStringLiteral("name"), object.name() },
				{ QStringLiteral("host"), host },
				{ QStringLiteral("mac"), object.macAddress() },
				{ QStringLiteral("location"), location },
			} );
		}
	}

	return QJsonDocument( hosts ).toJson( QJsonDocument::Compact );
}



void GatewayService::wakeOnLan( const QString& macAddress )
{
	auto mac = QByteArray::fromHex( macAddress.toLatin1().replace( ':', "" ).replace( '-', "" ) );
	if( mac.size() != 6 )
	{
		return;
	}

	QByteArray packet( 6, char( 0xff ) );
	for( int i = 0; i < 16; ++i )
	{
		packet.append( mac );
	}

	QUdpSocket socket;
	socket.writeDatagram( packet, QHostAddress::Broadcast, 9 );
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		for( const auto& entry : iface.addressEntries() )
		{
			if( entry.broadcast().isNull() == false )
			{
				socket.writeDatagram( packet, entry.broadcast(), 9 );
			}
		}
	}
	vInfo() << "Aruni Gateway: sent Wake-on-LAN packet to" << macAddress;
}



void GatewayService::checkState()
{
	const auto modified = QFileInfo( GatewayState::statePath() ).lastModified();
	const bool changed = modified != m_stateModified;
	if( changed )
	{
		const auto previous = m_state;
		m_state = GatewayState::load();
		m_stateModified = modified;

		const bool identityChanged = previous.gatewayId != m_state.gatewayId || previous.relayUrl != m_state.relayUrl ||
									 previous.relaySecret != m_state.relaySecret;
		if( identityChanged && m_control.state() != QAbstractSocket::UnconnectedState )
		{
			m_control.close();
		}
	}

	if( m_state.enabled == false )
	{
		if( m_haveLock )
		{
			m_control.close();
			m_reconnectTimer.stop();
			m_directoryTimer.stop();
			delete m_directory;
			m_directory = nullptr;
			GatewayState::writeStatus( { { QStringLiteral("running"), false }, { QStringLiteral("enabled"), false } } );
			m_instanceLock.unlock();
			m_haveLock = false;
		}
		return;
	}

	// only one server instance per computer acts as gateway
	if( m_haveLock == false )
	{
		m_haveLock = m_instanceLock.tryLock( 0 );
		if( m_haveLock == false )
		{
			return;
		}

		// persist a freshly created identity so the Configurator shows the same one
		GatewayState::update( []( GatewayState& ) {} );
		m_state = GatewayState::load();
		m_stateModified = QFileInfo( GatewayState::statePath() ).lastModified();

		m_directory = VeyonCore::networkObjectDirectoryManager().createDirectory( NetworkDiscoveryPluginUid, this );
		updateDirectory();
		m_directoryTimer.start( DirectoryUpdateInterval );
	}

	if( m_control.state() == QAbstractSocket::UnconnectedState && m_reconnectTimer.isActive() == false )
	{
		connectToRelay();
	}

	writeStatus();
}



void GatewayService::connectToRelay()
{
	if( m_haveLock == false || m_state.enabled == false )
	{
		return;
	}

	QUrl url( m_state.relayUrl );
	url.setPath( url.path() + QStringLiteral("/v1/gateway/") + m_state.gatewayId );

	QNetworkRequest request( url );
	request.setRawHeader( "X-Aruni-Secret", m_state.relaySecret.toUtf8() );
	m_control.open( request );
}



void GatewayService::onTextMessage( const QString& message )
{
	const auto json = QJsonDocument::fromJson( message.toUtf8() ).object();
	if( json[QStringLiteral("type")].toString() != QStringLiteral("session") )
	{
		return;
	}

	const auto sessionId = json[QStringLiteral("sid")].toString();
	if( sessionId.isEmpty() || sessionId.size() > 64 )
	{
		return;
	}

	QUrl url( m_state.relayUrl );
	url.setPath( url.path() + QStringLiteral("/v1/accept/%1/%2").arg( m_state.gatewayId, sessionId ) );

	auto session = new GatewaySession( this, sessionId );
	++m_sessions;
	connect( session, &GatewaySession::finished, this, [this]() {
		--m_sessions;
		writeStatus();
	} );
	session->start( url, m_state.relaySecret.toUtf8() );

	updateDirectory();
	writeStatus();
}



void GatewayService::onDisconnected()
{
	const bool wasConnected = m_connected;
	m_connected = false;

	if( m_lastError.isEmpty() && m_control.closeReason().isEmpty() == false )
	{
		m_lastError = m_control.closeReason();
	}

	if( wasConnected )
	{
		vWarning() << "Aruni Gateway lost the relay connection:" << m_lastError;
	}

	writeStatus();

	if( m_haveLock && m_state.enabled )
	{
		m_reconnectTimer.start( m_reconnectDelay );
		m_reconnectDelay = qMin( m_reconnectDelay * 2, MaxReconnectDelay );
	}
}



void GatewayService::updateDirectory()
{
	if( m_directory )
	{
		m_directory->update();
	}
}



void GatewayService::writeStatus()
{
	if( m_haveLock == false )
	{
		return;
	}

	GatewayState::writeStatus( {
		{ QStringLiteral("running"), true },
		{ QStringLiteral("enabled"), m_state.enabled },
		{ QStringLiteral("connected"), m_connected },
		{ QStringLiteral("error"), m_lastError },
		{ QStringLiteral("sessions"), m_sessions },
		{ QStringLiteral("id"), m_state.gatewayId },
		{ QStringLiteral("relay"), m_state.relayUrl },
		{ QStringLiteral("updated"), QDateTime::currentDateTimeUtc().toString( Qt::ISODate ) },
	} );
}



QStringList GatewayService::localSubnets() const
{
	QStringList subnets;
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
			if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol && entry.prefixLength() >= 16 )
			{
				const auto network = QHostAddress( entry.ip().toIPv4Address() & entry.netmask().toIPv4Address() );
				subnets.append( QStringLiteral("%1/%2").arg( network.toString() ).arg( entry.prefixLength() ) );
			}
		}
	}
	subnets.removeDuplicates();
	return subnets;
}
