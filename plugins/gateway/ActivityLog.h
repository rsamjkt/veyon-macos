/*
 * ActivityLog.h - history of the roaming laptops and phones of a gateway
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

// Append-only event log of the Aruni Gateway (one JSON object per line in
// <gateway directory>/activity.jsonl), rotated by size and pruned by age.
// Written by the gateway in veyon-server, read by the Configurator and CLI.
class ActivityLog
{
	Q_DECLARE_TR_FUNCTIONS(ActivityLog)
public:
	struct Entry
	{
		QDateTime time;
		QString event;     // e.g. "laptop.online", see describe()
		QString subject;   // laptop or phone name
		QJsonObject details;
	};

	static void append( const QString& event, const QString& subject, const QJsonObject& details = {} );
	// with the time the event happened (e.g. access logs collected later)
	static void appendAt( const QDateTime& time, const QString& event, const QString& subject, const QJsonObject& details = {} );

	// newest first; entries of the rotated file are included
	static QList<Entry> read( int maximum = 5000, const QDateTime& since = {} );

	static bool exportCsv( const QString& fileName, const QDateTime& since = {} );

	// human readable (Indonesian through the translation) description
	static QString describe( const Entry& entry );
	static QString eventName( const QString& event );

	static QString logPath();

	static constexpr qint64 MaxFileSize = 8 * 1024 * 1024;
	static constexpr int RetentionDays = 180;

};
