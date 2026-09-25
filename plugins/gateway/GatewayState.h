/*
 * GatewayState.h - persistent state of the Aruni Gateway
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
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

#include "AruniTunnel.h"

// Shared between the Configurator (admin: enable, pair/revoke phones) and the
// gateway running inside veyon-server. Kept in a JSON file below the global
// app data directory so both processes see changes without a restart.
class GatewayState
{
public:
	static constexpr auto DefaultRelayUrl = "wss://relay.arunika.id";
	static constexpr int PairingValiditySeconds = 15 * 60;

	struct Device
	{
		QByteArray publicKey;
		QString name;
		QDateTime added;
		QDateTime lastSeen;
	};

	bool enabled{false};
	QString relayUrl{QString::fromLatin1( DefaultRelayUrl )};
	QString siteName;
	QString gatewayId;
	QString relaySecret;
	AruniTunnel::KeyPair keyPair;
	QList<Device> devices;
	QByteArray pairingToken;
	QDateTime pairingExpires;
	// name of an authentication key (private key on this PC) that newly paired
	// phones receive, so they need no separate key import
	QString sharedKeyName;

	static QString directory();
	static QString statePath();
	static QString statusPath();

	// loads the state, creating identity/secrets on first use
	static GatewayState load();
	bool save() const;

	// modify-and-save under a lock file so Configurator and gateway don't clash
	static bool update( const std::function<void(GatewayState&)>& modifier );

	bool isPairingActive() const
	{
		return pairingToken.size() == AruniTunnel::TokenSize && pairingExpires > QDateTime::currentDateTimeUtc();
	}

	AruniTunnel::PairingInfo pairingInfo() const;

	// status reported by the running gateway
	static void writeStatus( const QJsonObject& status );
	static QJsonObject readStatus();

};
