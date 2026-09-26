/*
 * RoamingAgent.h - keeps a roaming laptop reachable through the office gateway
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

#include <QDateTime>
#include <QLockFile>
#include <QTimer>

#include <memory>

#include "GatewayState.h"
#include "TunnelEndpoint.h"

// Runs in the AruniControl server of a laptop that was enrolled with an office
// gateway. It keeps an encrypted session to that gateway through the relay -
// wherever the laptop is - and serves the connections the gateway forwards to
// it, which may only go to this computer's own AruniControl server.
class RoamingAgent : public TunnelEndpoint
{
	Q_OBJECT
public:
	explicit RoamingAgent( QObject* parent = nullptr );
	~RoamingAgent() override;

	static QJsonObject helloInfo();

protected:
	bool isAllowedTarget( const QHostAddress& address, quint16 port ) const override;

private:
	void checkState();
	void connectToGateway();
	void onConnected();
	void onBinaryMessage( const QByteArray& message );
	void onDisconnected();
	void sendHello();
	void writeStatus();

	static constexpr int StateCheckInterval = 3000;
	static constexpr int PingInterval = 40 * 1000;
	static constexpr int HelloInterval = 60 * 1000;
	static constexpr int MaxReconnectDelay = 60 * 1000;
	static constexpr int RejectedRetryDelay = 5 * 60 * 1000;
	static constexpr int HandshakeTimeout = 20 * 1000;

	QLockFile m_instanceLock;
	bool m_haveLock{false};

	GatewayState m_state;
	QDateTime m_stateModified;

	std::unique_ptr<AruniTunnel::MasterHandshake> m_handshake;
	bool m_rejected{false};
	bool m_relayConnected{false};
	int m_refusals{0};
	QString m_lastError;
	QString m_attemptError;
	QDateTime m_connectedSince;
	int m_reconnectDelay{2000};
	QTimer m_reconnectTimer;
	QTimer m_stateTimer;
	QTimer m_pingTimer;
	QTimer m_helloTimer;
	QTimer m_handshakeTimer;

};
