/*
 * InventoryStore.h - collected inventory of the computers
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

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>
#include <QList>

// The inventory the gateway collects (InventoryFeaturePlugin), one JSON file
// per computer in <gateway directory>/inventory - written by veyon-server,
// read by the Configurator and the CLI.
class InventoryStore
{
	Q_DECLARE_TR_FUNCTIONS(InventoryStore)
public:
	struct Record
	{
		QString key;			// "lan:<address>" / "agent:<id>"
		QString name;
		QString host;
		bool roaming{false};
		QDateTime lastSeen;
		QDateTime collected;
		QJsonObject data;		// see InventoryFeaturePlugin::collect()
		bool offlineAlerted{false};
		QString diskAlertDate;

		QString fileName() const;
		// lowest free space of the disks in percent, -1 if unknown
		int lowestFreePercent() const;
	};

	static QString directory();
	static QList<Record> list();
	static Record load( const QString& key );
	static bool save( const Record& record );

	// computers: one row each; software: one row per computer and program
	static bool exportComputersCsv( const QString& fileName );
	static bool exportSoftwareCsv( const QString& fileName );

};
