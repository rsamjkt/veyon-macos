/*
 * TunnelEndpoint.h - one end of an encrypted relay session carrying TCP streams
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

#pragma once

#include <QHash>
#include <QHostAddress>
#include <QPointer>
#include <QSet>
#include <QTcpSocket>
#include <QWebSocket>

#include "AruniTunnel.h"

// Stream plumbing shared by the gateway side of a session (GatewaySession) and
// a roaming laptop (AgentLink): TCP connections are multiplexed as numbered
// streams over the encrypted WebSocket, with flow control so a slow relay makes
// TCP slow the sender down instead of buffering without limit.
class TunnelEndpoint : public QObject
{
	Q_OBJECT
public:
	explicit TunnelEndpoint( QObject* parent = nullptr );
	~TunnelEndpoint() override;

protected:
	void sendFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload = {} );

	// Open frame from the peer: connect to port(2) | host, if allowed
	void openStream( quint32 stream, const QByteArray& payload );
	// a locally accepted connection that the peer is asked to open (reverse
	// direction); data is only read once the peer confirms with Opened
	void attachStream( quint32 stream, QTcpSocket* socket );
	void closeStream( quint32 stream, bool notify );
	void closeAllStreams();
	// for endpoints reusing their WebSocket: drops all streams and the flow
	// control state of the previous connection
	void resetStreams();

	// Opened/Data/Close; returns false for other frame types
	bool handleStreamFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload );

	virtual bool isAllowedTarget( const QHostAddress& address, quint16 port ) const = 0;
	// lets a subclass map a host name to a target of its own (0 = not mapped)
	virtual quint16 mapTarget( const QString& host, quint16 port, QHostAddress& target ) const;

	int streamCount() const
	{
		return int( m_streams.size() );
	}

	QWebSocket m_socket;
	AruniTunnel::SecureChannel m_channel;
	bool m_established{false};
	bool m_finished{false};

	static constexpr int MaxStreams = 256;

private:
	void setupSocket( quint32 stream, QTcpSocket* socket );
	void readFromStream( quint32 stream );
	void resumeStreams();

	static constexpr qint64 MaxPendingBytes = 1024 * 1024;
	static constexpr int ReadChunkSize = 32 * 1024;

	QHash<quint32, QPointer<QTcpSocket>> m_streams;
	QSet<quint32> m_awaitingOpen;
	qint64 m_pendingBytes{0};
	// invalidates name lookups still running for a previous connection
	quint64 m_epoch{0};

};
