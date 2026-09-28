/*
 * Reports.h - attendance, application usage and the daily report
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
#include <QHash>
#include <QList>
#include <QTime>

// Evaluations of what the gateway collects:
// - attendance from the "user_login"/"user_logout" events of the computers'
//   access logs (appmonitoring/SessionTracker)
// - application usage from the inventory (last 8 days per computer)
// - the daily report sent to Telegram
class Reports
{
	Q_DECLARE_TR_FUNCTIONS(Reports)
public:
	struct Session
	{
		QString user;
		QString computer;
		QDateTime start;
		QDateTime end;		// invalid: still logged on / unknown
		bool open{false};	// no logoff seen yet

		qint64 seconds() const;
	};

	// sessions that started in [from, to), oldest first
	static QList<Session> attendance( const QDate& from, const QDate& to );

	struct Usage
	{
		QString computer;
		QString user;
		QString application;
		int seconds{0};
	};

	// per computer, user and application, days in [from, to]
	static QList<Usage> usage( const QDate& from, const QDate& to );

	static bool exportAttendanceCsv( const QString& fileName, const QList<Session>& sessions );
	static bool exportUsageCsv( const QString& fileName, const QList<Usage>& usage );

	static QString duration( qint64 seconds );

	// Telegram text for the given day
	static QString dailyReport( const QDate& date );

	// settings/state of the daily report (<gateway directory>/daily-report.json)
	struct DailyReportSettings
	{
		bool enabled{false};
		QTime time{17, 0};
		QString lastSent;		// yyyy-MM-dd
	};
	static DailyReportSettings loadDailyReportSettings();
	static void saveDailyReportSettings( const DailyReportSettings& settings );
	static void requestDailyReport();
	static bool takeDailyReportRequest();

};
