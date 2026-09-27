/*
 * MonitoringCollector.h - screenshots and access logs of the computers
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
#include <QHash>
#include <QJsonObject>
#include <QTimer>

#include "ComputerControlInterface.h"

class GatewayService;

// The Aruni Gateway connects to the computers like a Master would - to the
// roaming laptops through their port forwarding, optionally to every computer
// of the office network - and
// - takes a screenshot every N minutes (screenshots/<computer>/<date>/<time>.jpg,
//   played back as a timelapse in the Configurator)
// - collects their access logs into the gateway's activity history
// It needs a private authentication key on this computer.
class MonitoringCollector : public QObject
{
	Q_OBJECT
public:
	explicit MonitoringCollector( GatewayService* service );
	~MonitoringCollector() override;

	static QString screenshotDirectory();

	// name of the key used, empty when this computer has no private key
	static QString availableKeyName( const QString& preferred );

	static QString folderName( const QString& computer );

	struct Target
	{
		QString key;		// stable: "agent:<id>" or "lan:<address>"
		QString name;
		QString host;
		quint16 port;
		bool roaming;
		bool local;			// roaming laptop that is in the office
	};

private:
	struct Visit
	{
		ComputerControlInterface::Pointer control;
		Target target;
		bool wantScreenshot{false};
		bool wantLog{false};
		bool wantInventory{false};
		QTimer* timeout{nullptr};
	};

	void tick();
	void visit( const Target& target, bool screenshot, bool log, bool inventory );
	void markSeen( const Target& target );
	void saveInventory( const Target& target, const QJsonObject& data );
	void checkOfflineComputers();
	void onFeatureMessage( const ComputerControlInterface::Pointer& control, const FeatureMessage& message );
	void finishPart( const QString& key );
	void finishVisit( const QString& key );
	void saveScreenshot( const Target& target, const QImage& image );
	void prune();
	bool loadCredentials();
	QString logCursor( const QString& key ) const;
	void setLogCursor( const QString& key, const QString& time );

	static constexpr int TickInterval = 60 * 1000;
	static constexpr int VisitTimeout = 60 * 1000;
	static constexpr int LogInterval = 15 * 60;
	static constexpr int InventoryInterval = 6 * 3600;
	static constexpr int MaxParallelVisits = 4;
	static constexpr int MaxImageWidth = 1600;

	GatewayService* m_service;
	QTimer m_timer;
	QHash<QString, QDateTime> m_lastScreenshot;
	QHash<QString, QDateTime> m_lastLog;
	QHash<QString, QDateTime> m_lastInventory;
	QHash<QString, QDateTime> m_lastSeenSaved;
	QDateTime m_lastOfflineCheck;
	QHash<QString, Visit> m_visits;
	QJsonObject m_cursors;
	QString m_loadedKey;
	QDate m_lastPrune;

};
