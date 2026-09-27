/*
 * Scheduler.h - runs the schedules on the computers
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
#include <QLockFile>
#include <QTimer>

#include "ComputerControlInterface.h"
#include "Schedules.h"

class NetworkObjectDirectory;
class Notifier;

// Runs the schedules of this computer inside veyon-server: at the set time it
// connects to the computers of the room (like a Master, with a private
// authentication key of this computer) and starts the function; "power on"
// sends Wake-on-LAN packets. The result goes to the activity history and, if
// set up, to Telegram. Only one server instance per computer runs schedules.
class Scheduler : public QObject
{
	Q_OBJECT
public:
	explicit Scheduler( QObject* parent = nullptr );
	~Scheduler() override;

private:
	struct Target
	{
		QString name;
		QString host;
		QString mac;
	};

	struct Run
	{
		Schedules::Rule rule;
		QList<Target> pending;
		int total{0};
		int succeeded{0};
		QStringList failed;
		int active{0};
	};

	struct Visit
	{
		ComputerControlInterface::Pointer control;
		QString runId;
		Target target;
		QTimer* timer{nullptr};
		bool sent{false};
	};

	void tick();
	void startRun( const Schedules::Rule& rule, bool manual );
	void continueRun( const QString& runId );
	void visit( const QString& runId, const Target& target );
	void finishVisit( ComputerControlInterface* control, bool ok );
	void finishRun( const QString& runId );
	bool sendAction( const Schedules::Rule& rule, const ComputerControlInterface::Pointer& control );
	QList<Target> targets( const QString& room );
	bool loadCredentials();
	static void wakeOnLan( const QString& macAddress );

	static constexpr int TickInterval = 20 * 1000;
	static constexpr int LateStartSeconds = 10 * 60;
	static constexpr int VisitTimeout = 45 * 1000;
	static constexpr int MaxParallelVisits = 8;

	QTimer m_timer;
	QLockFile m_lock;
	bool m_haveLock{false};
	NetworkObjectDirectory* m_directory{nullptr};
	Notifier* m_notifier{nullptr};
	QString m_loadedKey;
	QHash<QString, Run> m_runs;
	QHash<ComputerControlInterface*, Visit> m_visits;

};
