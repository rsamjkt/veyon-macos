/*
 * VpnController.h - remote access (WireGuard tunnel, ZeroTier, extra subnets)
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

#include <QObject>
#include <QStringList>
#include <QUrl>

// Lets the Master reach computers when the phone is not on their LAN, e.g. on
// mobile data:
// - an embedded WireGuard tunnel (config from MikroTik RouterOS v7 or any other
//   WireGuard server) that only carries AruniControl's own traffic
// - the official ZeroTier app, which the UI can open / install
// - extra subnets for network discovery, so computers behind the tunnel or a
//   router are found without adding them one by one
class VpnController : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString state READ state NOTIFY stateChanged)          // off, connecting, on, error
	Q_PROPERTY(QString error READ error NOTIFY stateChanged)
	Q_PROPERTY(bool supported READ isSupported CONSTANT)
	Q_PROPERTY(bool hasConfig READ hasConfig NOTIFY configChanged)
	Q_PROPERTY(QString endpoint READ endpoint NOTIFY configChanged)
	Q_PROPERTY(QString address READ address NOTIFY configChanged)
	Q_PROPERTY(QStringList tunnelSubnets READ tunnelSubnets NOTIFY configChanged)
	Q_PROPERTY(QStringList extraSubnets READ extraSubnets WRITE setExtraSubnets NOTIFY extraSubnetsChanged)
	Q_PROPERTY(bool otherVpnActive READ isOtherVpnActive NOTIFY stateChanged)
	Q_PROPERTY(bool zeroTierInstalled READ isZeroTierInstalled NOTIFY stateChanged)
	Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY configChanged)
public:
	explicit VpnController( QObject* parent = nullptr );

	static VpnController* instance()
	{
		return s_instance;
	}

	const QString& state() const
	{
		return m_state;
	}
	const QString& error() const
	{
		return m_error;
	}

	bool isSupported() const;
	bool hasConfig() const
	{
		return m_config.isEmpty() == false;
	}
	QString endpoint() const;
	QString address() const;
	QStringList tunnelSubnets() const;

	QStringList extraSubnets() const;
	void setExtraSubnets( const QStringList& subnets );

	bool isOtherVpnActive() const;
	bool isZeroTierInstalled() const;

	bool autoConnect() const;
	void setAutoConnect( bool enabled );

	Q_INVOKABLE QString importConfigText( const QString& text );
	Q_INVOKABLE QString importConfigFile( const QUrl& fileUrl );
	Q_INVOKABLE void removeConfig();
	Q_INVOKABLE void connectTunnel();
	Q_INVOKABLE void disconnectTunnel();
	Q_INVOKABLE void refresh();
	Q_INVOKABLE void openZeroTier();
	Q_INVOKABLE QString validateSubnet( const QString& subnet ) const;

	// called from the Java side (any thread)
	void onTunnelStateChanged( bool up, const QString& error );

Q_SIGNALS:
	void stateChanged();
	void configChanged();
	void extraSubnetsChanged();
	void networkRoutesChanged();

private:
	QString configPath() const;
	QString value( const QString& section, const QString& key ) const;
	static QString withOwnPackageOnly( const QString& config );
	void setState( const QString& state, const QString& error = {} );
	void applyDiscoverySubnets();
	void startTunnel();

	static VpnController* s_instance;

	QString m_config;
	QString m_state{QStringLiteral("off")};
	QString m_error;

};
