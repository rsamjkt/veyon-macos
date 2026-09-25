/*
 * GatewayManager.h - connects the mobile Master to Aruni Gateways via the relay
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
#include <QJsonArray>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVariant>
#include <QWebSocket>

#include <memory>

#include "AruniTunnel.h"

class GatewayManager;

// A saved location: which gateway to reach through which relay
struct GatewaySite
{
	QString relayUrl;
	QString gatewayId;
	QString name;
	QByteArray publicKey;
	QByteArray pairingToken; // set until the first successful connection
};


// Encrypted session to one gateway. Connections of the Master to computers
// behind it arrive on local listeners (see GatewayManager::redirect()) and are
// multiplexed as streams over the session.
class GatewayLink : public QObject
{
	Q_OBJECT
public:
	GatewayLink( GatewayManager* manager, const GatewaySite& site, const AruniTunnel::KeyPair& device );
	~GatewayLink() override;

	const GatewaySite& site() const
	{
		return m_site;
	}

	const QString& state() const
	{
		return m_state;
	}
	const QString& error() const
	{
		return m_error;
	}
	const QJsonArray& hosts() const
	{
		return m_hosts;
	}
	bool isOnline() const
	{
		return m_state == QStringLiteral("online");
	}
	bool isOnSite() const;
	bool covers( const QString& host ) const;

	void start();
	void stop();

	// main thread only
	int localPortFor( const QString& host, int port );
	void wake( const QString& macAddress );

Q_SIGNALS:
	void stateChanged();
	void hostsChanged();
	void paired();
	void accessKeyReceived( const QString& name, const QString& pem );

private:
	void setState( const QString& state, const QString& error = {} );
	void onConnected();
	void onBinaryMessage( const QByteArray& message );
	void onDisconnected();
	void handleFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload );
	void acceptLocalConnection( QTcpServer* server, const QString& host, int port );
	void forwardLocalData( quint32 stream );
	void closeStream( quint32 stream, bool notify );
	void sendFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload = {} );
	void closeListeners();
	void scheduleReconnect();

	struct Stream
	{
		QPointer<QTcpSocket> socket;
		bool opened{false};
	};

	static constexpr int ReadChunkSize = 32 * 1024;
	static constexpr qint64 MaxPendingBytes = 1024 * 1024;

	GatewayManager* m_manager;
	GatewaySite m_site;
	AruniTunnel::KeyPair m_device;
	QWebSocket m_socket;
	std::unique_ptr<AruniTunnel::MasterHandshake> m_handshake;
	AruniTunnel::SecureChannel m_channel;
	bool m_established{false};
	bool m_running{false};
	bool m_pairing{false};
	QString m_state{QStringLiteral("offline")};
	QString m_error;
	QJsonArray m_hosts;
	QStringList m_subnets;
	QHash<QString, QTcpServer*> m_listeners;
	QHash<quint32, Stream> m_streams;
	quint32 m_nextStream{1};
	qint64 m_pendingBytes{0};
	QTimer m_pingTimer;
	QTimer m_hostsTimer;
	QTimer m_reconnectTimer;
	int m_reconnectDelay{2000};
	qint64 m_lastPong{0};

};


class GatewayManager : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QVariantList sites READ sites NOTIFY sitesChanged)
	Q_PROPERTY(bool scanSupported READ isScanSupported CONSTANT)
public:
	explicit GatewayManager( QObject* parent = nullptr );
	~GatewayManager() override;

	static GatewayManager* instance()
	{
		return s_instance;
	}

	QVariantList sites() const;
	bool isScanSupported() const;

	Q_INVOKABLE QString addFromCode( const QString& code );
	Q_INVOKABLE void removeSite( const QString& gatewayId );
	Q_INVOKABLE void reconnect( const QString& gatewayId );
	Q_INVOKABLE void scanQrCode();

	// computers of all online sites that are not reachable directly, for the directory
	QList<QPair<GatewaySite, QJsonArray>> remoteHosts() const;

	// VncConnection redirector - thread-safe
	bool redirect( const QString& host, int port, QString& redirectedHost, int& redirectedPort );

	// Wake-on-LAN through the gateway covering the host; false if none does
	bool wake( const QString& host, const QString& macAddress );

	void handleScannedCode( const QString& code, const QString& error );

Q_SIGNALS:
	void sitesChanged();
	void remoteHostsChanged();
	void accessKeyReceived( const QString& name, const QString& pem );
	void notify( const QString& message, const QString& kind );
	void siteAdded( const QString& name );

private:
	void load();
	void save();
	void startLink( const GatewaySite& site );
	GatewayLink* linkCovering( const QString& host ) const;

	static GatewayManager* s_instance;

	AruniTunnel::KeyPair m_device;
	QList<GatewaySite> m_sites;
	QHash<QString, GatewayLink*> m_links;

};
