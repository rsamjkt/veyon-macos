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
	static constexpr auto DefaultRelayUrl = "wss://relay.arunihealth.id";
	static constexpr int PairingValiditySeconds = 15 * 60;

	struct Device
	{
		QByteArray publicKey;
		QString name;
		QDateTime added;
		QDateTime lastSeen;
	};

	// a roaming laptop registered with this gateway (hub side)
	struct Agent
	{
		QByteArray publicKey;
		QString name;
		// the gateway forwards connections to agentPort( slot ) to the laptop,
		// so Masters on the office LAN reach it as "<gateway>:<port>"
		int slot{0};
		QDateTime added;
		QDateTime lastSeen;
		// an "offline for too long" alert went out and "back online" is due
		bool alerted{false};

		// short, stable host name under which the app lists the laptop
		QString id() const;
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

	// hub side: laptops enroll with this (reusable) token until it is renewed
	QByteArray enrollmentToken;
	QList<Agent> agents;

	// alerts to the admin's phone through a Telegram bot
	QString telegramToken;
	QString telegramChatId;
	int offlineAlertHours{0};		// 0 = no alert
	bool notifyRefused{true};
	bool notifyNewLaptop{true};

	// periodic screenshots of the roaming laptops
	int screenshotInterval{0};		// minutes, 0 = off
	int screenshotRetentionDays{30};
	bool screenshotInOffice{false};	// also while the laptop is in the office
	bool screenshotAllComputers{false};	// every computer of the office, not only roaming laptops

	// access logs of the computers collected into the activity history
	bool collectAccessLogs{true};

	bool isTelegramConfigured() const
	{
		return telegramToken.isEmpty() == false && telegramChatId.isEmpty() == false;
	}

	// agent side: this computer is a roaming laptop of the given office gateway
	bool roamingEnabled{false};
	AruniTunnel::PairingInfo roamingHub;
	AruniTunnel::KeyPair roamingKeyPair;
	bool roamingRegistered{false};

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
	AruniTunnel::PairingInfo enrollmentInfo() const;

	// first free port slot for a new roaming laptop, 0 if none left
	int freeAgentSlot() const;

	static constexpr int MaxAgentSlot = 98;
	// Roaming laptops are forwarded on <Veyon server port> + 500 + slot, i.e.
	// 11601-11698 by default - clear of the ports Veyon itself uses per session
	// (server, VNC, feature worker manager and demo server, 11100-11499). The
	// service listing them for Masters on the office LAN (see
	// NetworkDiscoveryDirectory) is on + 599.
	static constexpr int RoamingPortOffset = 500;
	static constexpr int DirectoryPortOffset = RoamingPortOffset + 99;
	static quint16 agentPort( int slot );
	static quint16 directoryPort();

	// status reported by the running gateway / roaming agent
	static void writeStatus( const QJsonObject& status, const QString& fileName = QStringLiteral("status.json") );
	static QJsonObject readStatus( const QString& fileName = QStringLiteral("status.json") );

};
