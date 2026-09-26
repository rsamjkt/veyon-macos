/*
 * GatewaySession.h - one Master or roaming laptop connected through the relay
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

#include <QTimer>

#include "TunnelEndpoint.h"

class GatewayService;

// Terminates the encrypted tunnel of one peer:
// - a Master: its streams become TCP connections to AruniControl servers on
//   the local network
// - a roaming laptop (agent): it stays connected, and connections to its port
//   on this gateway are forwarded to the laptop's AruniControl server
class GatewaySession : public TunnelEndpoint
{
	Q_OBJECT
public:
	GatewaySession( GatewayService* service, const QString& sessionId );
	~GatewaySession() override = default;

	void start( const QUrl& acceptUrl, const QByteArray& relaySecret );

	bool isAgent() const
	{
		return m_agent;
	}

	const QByteArray& peerKey() const
	{
		return m_deviceKey;
	}

	// agent sessions: forward a connection to the laptop's server
	void forwardToAgent( QTcpSocket* socket );

	void finish();

Q_SIGNALS:
	void finished();

protected:
	bool isAllowedTarget( const QHostAddress& address, quint16 port ) const override;
	quint16 mapTarget( const QString& host, quint16 port, QHostAddress& target ) const override;

private:
	void onBinaryMessage( const QByteArray& message );
	void handleFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload );

	static constexpr int HandshakeTimeout = 15000;

	GatewayService* m_service;
	QString m_sessionId;
	AruniTunnel::GatewayHandshake m_handshake;
	bool m_pairedNow{false};
	bool m_agent{false};
	quint32 m_nextAgentStream{1};
	QByteArray m_deviceKey;
	QTimer m_handshakeTimer;

};
