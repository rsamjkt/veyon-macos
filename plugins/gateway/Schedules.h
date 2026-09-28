/*
 * Schedules.h - automatic actions at set times
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
#include <QStringList>
#include <QTime>

// Actions done automatically at set times (power on in the morning, lock the
// screens during the break, block the internet, shut down in the evening...),
// run by the AruniControl server of the admin computer (see Scheduler).
// Stored in <gateway directory>/schedules.json, written by the Configurator.
class Schedules
{
	Q_DECLARE_TR_FUNCTIONS(Schedules)
public:
	enum class Action
	{
		PowerOn,
		PowerDown,
		Reboot,
		LockScreen,
		UnlockScreen,
		BlockInternet,
		AllowInternet,
		BlockSites,
		UnblockSites,
		Message,
		StartExam,
		EndExam,
		BlockUsb,
		AllowUsb,
		BlockPrinting,
		AllowPrinting,
		// not offered for schedules: sends the admin roles (AdminRoles) to
		// every computer, requested with requestRun( PushRolesId )
		PushRoles,
	};

	static constexpr auto PushRolesId = "@roles";

	struct Rule
	{
		QString id;
		QString name;
		bool enabled{true};
		int days{0x1f};			// bit 0 = Monday ... bit 6 = Sunday
		QTime time{7, 0};
		Action action{Action::PowerOn};
		QString room;			// empty = all computers
		QStringList sites;		// BlockSites, StartExam
		QString text;			// Message; StartExam: website to open

		bool runsOn( const QDate& date ) const
		{
			return days & ( 1 << ( date.dayOfWeek() - 1 ) );
		}
	};

	QList<Rule> rules;

	static Schedules load();
	bool save() const;
	static QString path();

	static QList<Action> actions();
	static QString actionKey( Action action );
	static Action actionFromKey( const QString& key, bool* ok = nullptr );
	static QString actionName( Action action );
	static QString daysText( int days );
	static bool needsSites( Action action );

	// the Configurator asks the scheduler to run a rule right away
	static void requestRun( const QString& ruleId );
	static QStringList takeRunRequests();

	// last result per rule, written by the scheduler: { id: { time, text } }
	static QJsonObject readStatus();
	static void writeStatus( const QJsonObject& status );

};
