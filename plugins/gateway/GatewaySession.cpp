/*
 * GatewaySession.cpp - one Master or roaming laptop connected through the relay
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

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QtEndian>

#include "GatewayService.h"
#include "GatewaySession.h"
#include "VeyonConfiguration.h"


GatewaySession::GatewaySession( GatewayService* service, const QString& sessionId ) :
	TunnelEndpoint( service ),
	m_service( service ),
	m_sessionId( sessionId ),
	m_handshake( service->keyPair() )
{
	connect( &m_socket, &QWebSocket::binaryMessageReceived, this, &GatewaySession::onBinaryMessage );
	connect( &m_socket, &QWebSocket::disconnected, this, &GatewaySession::finish );

	m_handshakeTimer.setSingleShot( true );
	connect( &m_handshakeTimer, &QTimer::timeout, this, [this]() {
		if( m_established == false )
		{
			vWarning() << "gateway session" << m_sessionId << "handshake timed out";
			finish();
		}
	} );
}



void GatewaySession::start( const QUrl& acceptUrl, const QByteArray& relaySecret )
{
	QNetworkRequest request( acceptUrl );
	request.setRawHeader( "X-Aruni-Secret", relaySecret );
	m_socket.open( request );
	m_handshakeTimer.start( HandshakeTimeout );
}



void GatewaySession::forwardToAgent( QTcpSocket* socket )
{
	if( m_agent == false || m_established == false || streamCount() >= MaxStreams )
	{
		socket->abort();
		socket->deleteLater();
		return;
	}

	// streams of agent sessions are only ever opened by the gateway
	const auto stream = m_nextAgentStream++;
	attachStream( stream, socket );

	QByteArray payload( 2, 0 );
	qToBigEndian( quint16( VeyonCore::config().veyonServerPort() ), reinterpret_cast<uchar*>( payload.data() ) );
	payload.append( "127.0.0.1" );
	sendFrame( AruniTunnel::FrameType::Open, stream, payload );
}



void GatewaySession::onBinaryMessage( const QByteArray& message )
{
	if( m_established == false )
	{
		QByteArray reply;
		const auto authorizer = [this]( const QByteArray& deviceKey, const QByteArray& token, const QString& name ) {
			if( m_handshake.isAgent() )
			{
				return m_service->authorizeAgent( deviceKey, token, name );
			}
			const bool known = m_service->deviceName( deviceKey ).isEmpty() == false;
			const bool authorized = m_service->authorizeDevice( deviceKey, token, name );
			m_pairedNow = authorized && known == false;
			return authorized;
		};

		if( m_handshake.processFirstMessage( message, authorizer, m_service->gatewayInfo(), reply, m_channel ) == false )
		{
			vWarning() << "gateway session" << m_sessionId << "rejected (unknown device or invalid pairing code)";
			finish();
			return;
		}

		m_deviceKey = m_handshake.devicePublicKey();
		m_agent = m_handshake.isAgent();
		m_established = true;
		m_handshakeTimer.stop();
		m_socket.sendBinaryMessage( reply );

		if( m_agent )
		{
			vInfo() << "gateway session" << m_sessionId << "established for roaming laptop" << m_service->agentName( m_deviceKey );
			m_service->agentConnected( this );
		}
		else
		{
			vInfo() << "gateway session" << m_sessionId << "established for" << m_service->deviceName( m_deviceKey );
		}
		return;
	}

	QByteArray frame;
	if( m_channel.decrypt( message, frame ) == false )
	{
		vWarning() << "gateway session" << m_sessionId << "received an invalid message - closing";
		finish();
		return;
	}

	AruniTunnel::FrameType type;
	quint32 stream = 0;
	QByteArray payload;
	if( AruniTunnel::parseFrame( frame, type, stream, payload ) )
	{
		handleFrame( type, stream, payload );
	}
}



void GatewaySession::handleFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload )
{
	using AruniTunnel::FrameType;

	if( handleStreamFrame( type, stream, payload ) )
	{
		return;
	}

	if( type == FrameType::Ping )
	{
		sendFrame( FrameType::Pong, stream, payload );
		return;
	}

	if( m_agent )
	{
		// a laptop cannot reach anything through the gateway - it only reports
		// itself and serves the streams the gateway opens
		if( type == FrameType::AgentHello )
		{
			m_service->agentHello( m_deviceKey, QJsonDocument::fromJson( payload ).object() );
		}
		return;
	}

	switch( type )
	{
	case FrameType::Open:
		openStream( stream, payload );
		break;
	case FrameType::HostsRequest:
		sendFrame( FrameType::Hosts, stream, m_service->hostsJson() );
		break;
	case FrameType::Wake:
		m_service->wakeOnLan( QString::fromUtf8( payload ) );
		break;
	case FrameType::KeyRequest:
		// the access key is only handed out in the session that paired the
		// device (i.e. to whoever scanned the QR code the admin displayed)
		sendFrame( FrameType::Key, stream, m_pairedNow ? m_service->sharedKeyJson() : QByteArray{} );
		m_pairedNow = false;
		break;
	default:
		break;
	}
}



quint16 GatewaySession::mapTarget( const QString& host, quint16 port, QHostAddress& target ) const
{
	Q_UNUSED(port)

	// roaming laptops are reached through the local port forwarding to them
	if( m_agent == false )
	{
		if( const auto agentPort = m_service->agentPort( host ) )
		{
			target = QHostAddress::LocalHost;
			return agentPort;
		}
	}
	return 0;
}



bool GatewaySession::isAllowedTarget( const QHostAddress& address, quint16 port ) const
{
	if( m_agent )
	{
		return false;
	}

	// only AruniControl servers (one port per session) ...
	const auto basePort = VeyonCore::config().veyonServerPort();
	if( port < basePort || port >= basePort + 100 )
	{
		return false;
	}

	// ... on the local network - the gateway is no open proxy
	return GatewayService::isLocalNetworkAddress( address );
}



void GatewaySession::finish()
{
	if( m_finished )
	{
		return;
	}
	m_finished = true;

	closeAllStreams();

	if( m_agent )
	{
		m_service->agentDisconnected( this );
	}
	else if( m_deviceKey.isEmpty() == false )
	{
		m_service->markDeviceSeen( m_deviceKey );
	}

	m_socket.close();
	Q_EMIT finished();
	deleteLater();
}
