/*
 * SessionTracker.h - attendance and application usage of this computer
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

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QTimer>

// Runs in veyon-server (one per user session):
// - attendance: who logs on and off, written to the access log ("user_login",
//   "user_logout"; a logoff missed because the computer was switched off is
//   written at the next start with the time of the last activity in "at")
// - application usage: every 15 s the application in the foreground gets the
//   time, unless the user was idle for 2 minutes. Per day and user in
//   %GLOBALAPPDATA%/logs/usage/<yyyy-MM-dd>.json { user: { application: seconds } },
//   kept 60 days. Only application names are recorded - no window titles,
//   no typing.
class SessionTracker : public QObject
{
	Q_OBJECT
public:
	explicit SessionTracker( QObject* parent = nullptr );
	~SessionTracker() override;

	static QString usageDirectory();
	// { "yyyy-MM-dd": { user: { application: seconds } } } of the last days
	static QJsonObject readUsage( int days );

private:
	void sample();
	void flush();
	void saveState();
	void finishPreviousSession();

	static constexpr int SampleInterval = 15;
	static constexpr int IdleLimit = 120;
	static constexpr int FlushInterval = 60 * 1000;
	static constexpr int KeepDays = 60;

	QTimer m_sampleTimer;
	QTimer m_flushTimer;
	QString m_user;
	QDateTime m_lastActive;
	QDate m_pendingDate;
	QHash<QString, QHash<QString, int>> m_pending;

};
