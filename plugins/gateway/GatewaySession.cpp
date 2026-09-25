/*
 * GatewaySession.cpp - one Master connected through the relay
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

#include <QHostInfo>
#include <QNetworkRequest>
#include <QtEndian>

#include "GatewayService.h"
#include "GatewaySession.h"
#include "VeyonConfiguration.h"


GatewaySession::GatewaySession( GatewayService* service, const QString& sessionId ) :
	QObject( service ),
	m_service( service ),
	m_sessionId( sessionId ),
	m_handshake( service->keyPair() )
{
	connect( &m_socket, &QWebSocket::binaryMessageReceived, this, &GatewaySession::onBinaryMessage );
	connect( &m_socket, &QWebSocket::disconnected, this, &GatewaySession::finish );
	connect( &m_socket, &QWebSocket::bytesWritten, this, [this]( qint64 bytes ) {
		m_pendingBytes = qMax<qint64>( 0, m_pendingBytes - bytes );
		if( m_pendingBytes < MaxPendingBytes / 2 )
		{
			resumeStreams();
		}
	} );

	m_handshakeTimer.setSingleShot( true );
	connect( &m_handshakeTimer, &QTimer::timeout, this, [this]() {
		if( m_established == false )
		{
			vWarning() << "gateway session" << m_sessionId << "handshake timed out";
			finish();
		}
	} );
}



GatewaySession::~GatewaySession()
{
	for( auto it = m_streams.begin(); it != m_streams.end(); ++it )
	{
		if( *it )
		{
			( *it )->disconnect( this );
			( *it )->abort();
			( *it )->deleteLater();
		}
	}
}



void GatewaySession::start( const QUrl& acceptUrl, const QByteArray& relaySecret )
{
	QNetworkRequest request( acceptUrl );
	request.setRawHeader( "X-Aruni-Secret", relaySecret );
	m_socket.open( request );
	m_handshakeTimer.start( HandshakeTimeout );
}



void GatewaySession::onBinaryMessage( const QByteArray& message )
{
	if( m_established == false )
	{
		QByteArray reply;
		const auto authorizer = [this]( const QByteArray& deviceKey, const QByteArray& token, const QString& name ) {
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
		m_established = true;
		m_handshakeTimer.stop();
		m_socket.sendBinaryMessage( reply );
		vInfo() << "gateway session" << m_sessionId << "established for" << m_service->deviceName( m_deviceKey );
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

	switch( type )
	{
	case FrameType::Open:
		openStream( stream, payload );
		break;
	case FrameType::Data:
		if( auto socket = m_streams.value( stream ) )
		{
			socket->write( payload );
		}
		break;
	case FrameType::Close:
		closeStream( stream, false );
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
	case FrameType::Ping:
		sendFrame( FrameType::Pong, stream, payload );
		break;
	default:
		break;
	}
}



void GatewaySession::openStream( quint32 stream, const QByteArray& payload )
{
	if( payload.size() < 3 || m_streams.contains( stream ) || m_streams.size() >= MaxStreams )
	{
		sendFrame( AruniTunnel::FrameType::Close, stream );
		return;
	}

	const auto port = qFromBigEndian<quint16>( reinterpret_cast<const uchar*>( payload.constData() ) );
	const auto host = QString::fromUtf8( payload.mid( 2 ) );

	// reserve the stream id so data arriving during the name lookup is dropped
	// instead of opening a second connection
	m_streams.insert( stream, nullptr );

	QHostInfo::lookupHost( host, this, [this, stream, port, host]( const QHostInfo& info ) {
		if( m_streams.contains( stream ) == false )
		{
			return;
		}

		QHostAddress target;
		for( const auto& address : info.addresses() )
		{
			if( isAllowedTarget( address, port ) )
			{
				target = address;
				break;
			}
		}

		if( target.isNull() )
		{
			vWarning() << "gateway session" << m_sessionId << "refused connection to" << host << port;
			m_streams.remove( stream );
			sendFrame( AruniTunnel::FrameType::Close, stream, QByteArrayLiteral("not allowed") );
			return;
		}

		auto socket = new QTcpSocket( this );
		socket->setSocketOption( QAbstractSocket::LowDelayOption, 1 );
		socket->setReadBufferSize( 4 * ReadChunkSize );
		m_streams.insert( stream, socket );

		connect( socket, &QTcpSocket::connected, this, [this, stream]() {
			sendFrame( AruniTunnel::FrameType::Opened, stream );
		} );
		connect( socket, &QTcpSocket::readyRead, this, [this, stream]() { readFromStream( stream ); } );
		connect( socket, &QTcpSocket::disconnected, this, [this, stream]() {
			readFromStream( stream );
			closeStream( stream, true );
		} );
		connect( socket, &QTcpSocket::errorOccurred, this, [this, stream]( QAbstractSocket::SocketError error ) {
			if( error != QAbstractSocket::RemoteHostClosedError )
			{
				closeStream( stream, true );
			}
		} );

		socket->connectToHost( target, port );
	} );
}



void GatewaySession::closeStream( quint32 stream, bool notify )
{
	if( m_streams.contains( stream ) == false )
	{
		return;
	}

	auto socket = m_streams.take( stream );
	if( socket )
	{
		socket->disconnect( this );
		socket->abort();
		socket->deleteLater();
	}

	if( notify )
	{
		sendFrame( AruniTunnel::FrameType::Close, stream );
	}
}



void GatewaySession::readFromStream( quint32 stream )
{
	auto socket = m_streams.value( stream );
	while( socket && socket->bytesAvailable() > 0 && m_pendingBytes < MaxPendingBytes )
	{
		sendFrame( AruniTunnel::FrameType::Data, stream, socket->read( ReadChunkSize ) );
	}
	// remaining data stays in the socket buffer (bounded by setReadBufferSize,
	// which makes TCP slow the sender down) until the relay caught up
}



void GatewaySession::sendFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload )
{
	if( m_established == false || m_finished )
	{
		return;
	}

	const auto message = m_channel.encrypt( AruniTunnel::makeFrame( type, stream, payload ) );
	m_pendingBytes += message.size();
	m_socket.sendBinaryMessage( message );
}



void GatewaySession::resumeStreams()
{
	const auto streams = m_streams.keys();
	for( const auto stream : streams )
	{
		readFromStream( stream );
	}
}



bool GatewaySession::isAllowedTarget( const QHostAddress& address, quint16 port ) const
{
	// only AruniControl servers (one port per session) ...
	const auto basePort = VeyonCore::config().veyonServerPort();
	if( port < basePort || port >= basePort + 100 )
	{
		return false;
	}

	// ... on the local network - the gateway is no open proxy
	if( address.isLoopback() )
	{
		return true;
	}

	if( address.protocol() == QAbstractSocket::IPv4Protocol )
	{
		for( const auto& subnet : { QStringLiteral("10.0.0.0/8"), QStringLiteral("172.16.0.0/12"),
									QStringLiteral("192.168.0.0/16"), QStringLiteral("100.64.0.0/10"),
									QStringLiteral("169.254.0.0/16") } )
		{
			if( address.isInSubnet( QHostAddress::parseSubnet( subnet ) ) )
			{
				return true;
			}
		}
		return false;
	}

	return address.isInSubnet( QHostAddress::parseSubnet( QStringLiteral("fc00::/7") ) ) ||
		   address.isLinkLocal();
}



void GatewaySession::finish()
{
	if( m_finished )
	{
		return;
	}
	m_finished = true;

	const auto streams = m_streams.keys();
	for( const auto stream : streams )
	{
		closeStream( stream, false );
	}

	if( m_deviceKey.isEmpty() == false )
	{
		m_service->markDeviceSeen( m_deviceKey );
	}

	m_socket.close();
	Q_EMIT finished();
	deleteLater();
}
