/*
 * GatewaySession.h - one Master connected through the relay
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
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>
#include <QWebSocket>

#include "AruniTunnel.h"

class GatewayService;

// Terminates the encrypted tunnel of one Master and forwards its streams as TCP
// connections to AruniControl servers on the local network
class GatewaySession : public QObject
{
	Q_OBJECT
public:
	GatewaySession( GatewayService* service, const QString& sessionId );
	~GatewaySession() override;

	void start( const QUrl& acceptUrl, const QByteArray& relaySecret );

Q_SIGNALS:
	void finished();

private:
	void onBinaryMessage( const QByteArray& message );
	void handleFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload );
	void openStream( quint32 stream, const QByteArray& payload );
	void closeStream( quint32 stream, bool notify );
	void readFromStream( quint32 stream );
	void sendFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload = {} );
	void resumeStreams();
	bool isAllowedTarget( const QHostAddress& address, quint16 port ) const;
	void finish();

	static constexpr qint64 MaxPendingBytes = 1024 * 1024;
	static constexpr int ReadChunkSize = 32 * 1024;
	static constexpr int MaxStreams = 256;
	static constexpr int HandshakeTimeout = 15000;

	GatewayService* m_service;
	QString m_sessionId;
	QWebSocket m_socket;
	AruniTunnel::GatewayHandshake m_handshake;
	AruniTunnel::SecureChannel m_channel;
	bool m_established{false};
	bool m_pairedNow{false};
	QByteArray m_deviceKey;
	QHash<quint32, QPointer<QTcpSocket>> m_streams;
	qint64 m_pendingBytes{0};
	QTimer m_handshakeTimer;
	bool m_finished{false};

};
