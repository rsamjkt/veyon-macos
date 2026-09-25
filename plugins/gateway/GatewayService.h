/*
 * GatewayService.h - Aruni Gateway running inside veyon-server
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
#include <QWebSocket>

#include <memory>

#include "GatewayState.h"

class NetworkObjectDirectory;

// Keeps an outbound control connection to the Aruni Relay, accepts Master
// sessions announced by the relay and lists the computers of this LAN.
// Only one gateway runs per computer (lock file), whichever server instance
// (user session) gets it first.
class GatewayService : public QObject
{
	Q_OBJECT
public:
	explicit GatewayService( QObject* parent = nullptr );
	~GatewayService() override;

	const AruniTunnel::KeyPair& keyPair() const
	{
		return m_state.keyPair;
	}

	bool authorizeDevice( const QByteArray& deviceKey, const QByteArray& pairingToken, const QString& deviceName );
	QString deviceName( const QByteArray& deviceKey ) const;
	void markDeviceSeen( const QByteArray& deviceKey );

	QByteArray gatewayInfo() const;
	QByteArray hostsJson() const;
	QByteArray sharedKeyJson() const;
	void wakeOnLan( const QString& macAddress );

private:
	void checkState();
	void connectToRelay();
	void onTextMessage( const QString& message );
	void onDisconnected();
	void updateDirectory();
	void writeStatus();
	QStringList localSubnets() const;

	static constexpr int StateCheckInterval = 3000;
	static constexpr int DirectoryUpdateInterval = 60 * 1000;
	static constexpr int MaxReconnectDelay = 60 * 1000;

	QLockFile m_instanceLock;
	bool m_haveLock{false};

	GatewayState m_state;
	QDateTime m_stateModified;

	QWebSocket m_control;
	bool m_connected{false};
	QString m_lastError;
	int m_reconnectDelay{2000};
	QTimer m_reconnectTimer;
	QTimer m_stateTimer;
	QTimer m_directoryTimer;
	int m_sessions{0};

	NetworkObjectDirectory* m_directory{nullptr};

};
