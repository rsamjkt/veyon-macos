/*
 * TunnelEndpoint.cpp - one end of an encrypted relay session carrying TCP streams
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
#include <QtEndian>

#include "TunnelEndpoint.h"
#include "VeyonCore.h"


TunnelEndpoint::TunnelEndpoint( QObject* parent ) :
	QObject( parent )
{
	connect( &m_socket, &QWebSocket::bytesWritten, this, [this]( qint64 bytes ) {
		m_pendingBytes = qMax<qint64>( 0, m_pendingBytes - bytes );
		if( m_pendingBytes < MaxPendingBytes / 2 )
		{
			resumeStreams();
		}
	} );
}



TunnelEndpoint::~TunnelEndpoint()
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



void TunnelEndpoint::sendFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload )
{
	if( m_established == false || m_finished )
	{
		return;
	}

	const auto message = m_channel.encrypt( AruniTunnel::makeFrame( type, stream, payload ) );
	m_pendingBytes += message.size();
	m_socket.sendBinaryMessage( message );
}



void TunnelEndpoint::openStream( quint32 stream, const QByteArray& payload )
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

	const auto connectTo = [this, stream]( const QHostAddress& target, quint16 targetPort ) {
		auto socket = new QTcpSocket( this );
		m_streams.insert( stream, socket );
		setupSocket( stream, socket );
		connect( socket, &QTcpSocket::connected, this, [this, stream]() {
			sendFrame( AruniTunnel::FrameType::Opened, stream );
		} );
		socket->connectToHost( target, targetPort );
	};

	QHostAddress mapped;
	if( const auto mappedPort = mapTarget( host, port, mapped ) )
	{
		connectTo( mapped, mappedPort );
		return;
	}

	QHostInfo::lookupHost( host, this, [this, stream, port, host, connectTo]( const QHostInfo& info ) {
		if( m_streams.contains( stream ) == false )
		{
			return;
		}

		for( const auto& address : info.addresses() )
		{
			if( isAllowedTarget( address, port ) )
			{
				connectTo( address, port );
				return;
			}
		}

		vWarning() << "tunnel: refused connection to" << host << port;
		m_streams.remove( stream );
		sendFrame( AruniTunnel::FrameType::Close, stream, QByteArrayLiteral("not allowed") );
	} );
}



void TunnelEndpoint::attachStream( quint32 stream, QTcpSocket* socket )
{
	socket->setParent( this );
	m_streams.insert( stream, socket );
	m_awaitingOpen.insert( stream );
	setupSocket( stream, socket );
}



void TunnelEndpoint::setupSocket( quint32 stream, QTcpSocket* socket )
{
	socket->setSocketOption( QAbstractSocket::LowDelayOption, 1 );
	socket->setReadBufferSize( 4 * ReadChunkSize );

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
}



void TunnelEndpoint::closeStream( quint32 stream, bool notify )
{
	if( m_streams.contains( stream ) == false )
	{
		return;
	}

	m_awaitingOpen.remove( stream );
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



void TunnelEndpoint::closeAllStreams()
{
	const auto streams = m_streams.keys();
	for( const auto stream : streams )
	{
		closeStream( stream, false );
	}
}



bool TunnelEndpoint::handleStreamFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload )
{
	using AruniTunnel::FrameType;

	switch( type )
	{
	case FrameType::Opened:
		if( m_awaitingOpen.remove( stream ) )
		{
			readFromStream( stream );
		}
		return true;
	case FrameType::Data:
		if( auto socket = m_streams.value( stream ) )
		{
			socket->write( payload );
		}
		return true;
	case FrameType::Close:
		closeStream( stream, false );
		return true;
	default:
		break;
	}

	return false;
}



quint16 TunnelEndpoint::mapTarget( const QString& host, quint16 port, QHostAddress& target ) const
{
	Q_UNUSED(host)
	Q_UNUSED(port)
	Q_UNUSED(target)
	return 0;
}



void TunnelEndpoint::readFromStream( quint32 stream )
{
	if( m_awaitingOpen.contains( stream ) )
	{
		return;
	}

	auto socket = m_streams.value( stream );
	while( socket && socket->bytesAvailable() > 0 && m_pendingBytes < MaxPendingBytes )
	{
		sendFrame( AruniTunnel::FrameType::Data, stream, socket->read( ReadChunkSize ) );
	}
	// remaining data stays in the socket buffer (bounded by setReadBufferSize,
	// which makes TCP slow the sender down) until the relay caught up
}



void TunnelEndpoint::resumeStreams()
{
	const auto streams = m_streams.keys();
	for( const auto stream : streams )
	{
		readFromStream( stream );
	}
}
