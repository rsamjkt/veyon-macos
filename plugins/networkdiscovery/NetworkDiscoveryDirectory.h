/*
 * NetworkDiscoveryDirectory.h - declaration of NetworkDiscoveryDirectory class
 *
 * Copyright (c) 2026 Arunika / AruniControl
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

#include <QHostAddress>
#include <QSet>

#include "NetworkObjectDirectory.h"

class QJsonArray;
class QTcpSocket;
class QTimer;

class NetworkDiscoveryDirectory : public NetworkObjectDirectory
{
	Q_OBJECT
public:
	explicit NetworkDiscoveryDirectory( QObject* parent );

	void update() override;

private:
	void startScan();
	void finishScan();
	void updateConfiguredObjects();
	void updateConfiguredLocation( const NetworkObject& locationObject, const QJsonArray& networkObjects );
	QList<QHostAddress> scanTargets() const;

	NetworkObject m_location;
	int m_serverPort;
	bool m_scanning{false};
	QSet<QString> m_foundHosts;
	// hosts kept across scans along with how many consecutive scans missed them
	QHash<QString, int> m_knownHosts;
	QList<QTcpSocket *> m_pendingSockets;
	QTimer* m_scanTimeout{nullptr};

	static constexpr int ScanTimeoutMs = 2000;
	// A host that fails to answer within ScanTimeoutMs would otherwise be dropped
	// from the directory straight away, destroying its ComputerControlInterface
	// and resetting the update mode of any session in progress back to
	// Monitoring - which silently downgrades a live remote view to the much
	// lower monitoring image quality. Tolerate a few misses instead.
	static constexpr int MaxMissedScans = 3;
	static constexpr int MaxHostsPerScan = 4096;

};
