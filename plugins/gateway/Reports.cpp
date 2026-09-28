/*
 * Reports.cpp - attendance, application usage and the daily report
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

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMap>

#include "ActivityLog.h"
#include "GatewayState.h"
#include "InventoryStore.h"
#include "Reports.h"
#include "Schedules.h"


namespace {

QString reportCsvField( QString value )
{
	value.replace( QLatin1Char('"'), QStringLiteral("\"\"") );
	if( value.contains( QLatin1Char(';') ) || value.contains( QLatin1Char('"') ) || value.contains( QLatin1Char('\n') ) )
	{
		value = QLatin1Char('"') + value + QLatin1Char('"');
	}
	return value;
}



bool writeReportCsv( const QString& fileName, const QList<QStringList>& rows )
{
	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return false;
	}
	file.write( "\xEF\xBB\xBF" );
	for( const auto& row : rows )
	{
		QStringList fields;
		for( const auto& field : row )
		{
			fields.append( reportCsvField( field ) );
		}
		file.write( fields.join( QLatin1Char(';') ).toUtf8() + "\r\n" );
	}
	return true;
}



QString dailyReportPath()
{
	return QDir( GatewayState::directory() ).filePath( QStringLiteral("daily-report.json") );
}



QString dailyReportRequestPath()
{
	return QDir( GatewayState::directory() ).filePath( QStringLiteral("daily-report-now") );
}

}



qint64 Reports::Session::seconds() const
{
	return end.isValid() ? qMax<qint64>( 0, start.secsTo( end ) ) : 0;
}



QList<Reports::Session> Reports::attendance( const QDate& from, const QDate& to )
{
	const QDateTime since( from.addDays( -1 ), QTime( 0, 0 ) );
	auto entries = ActivityLog::read( 200000, since.toUTC() );
	std::sort( entries.begin(), entries.end(), []( const ActivityLog::Entry& a, const ActivityLog::Entry& b ) {
		return a.time < b.time;
	} );

	QList<Session> sessions;
	QHash<QString, int> open;	// computer|user -> index in sessions

	for( const auto& entry : std::as_const( entries ) )
	{
		const bool login = entry.event == QStringLiteral("access.user_login");
		const bool logout = entry.event == QStringLiteral("access.user_logout");
		if( login == false && logout == false )
		{
			continue;
		}
		const auto user = entry.details[QStringLiteral("user")].toString();
		const auto key = entry.subject + QLatin1Char('|') + user;

		if( login )
		{
			// a second login without logoff in between: the first one ended unnoticed
			if( open.contains( key ) )
			{
				sessions[open.take( key )].open = false;
			}
			open[key] = int( sessions.size() );
			sessions.append( { user, entry.subject, entry.time, {}, true } );
		}
		else if( open.contains( key ) )
		{
			auto& session = sessions[open.take( key )];
			const auto at = QDateTime::fromString( entry.details[QStringLiteral("at")].toString(), Qt::ISODate );
			session.end = at.isValid() && at >= session.start ? at : entry.time;
			session.open = false;
		}
	}

	QList<Session> result;
	for( const auto& session : std::as_const( sessions ) )
	{
		const auto day = session.start.toLocalTime().date();
		if( day >= from && day <= to )
		{
			result.append( session );
		}
	}
	return result;
}



QList<Reports::Usage> Reports::usage( const QDate& from, const QDate& to )
{
	QMap<QString, Usage> totals;
	const auto records = InventoryStore::list();
	for( const auto& record : records )
	{
		const auto days = record.data[QStringLiteral("usage")].toObject();
		for( auto day = days.constBegin(); day != days.constEnd(); ++day )
		{
			const auto date = QDate::fromString( day.key(), Qt::ISODate );
			if( date < from || date > to )
			{
				continue;
			}
			const auto users = day.value().toObject();
			for( auto user = users.constBegin(); user != users.constEnd(); ++user )
			{
				const auto applications = user.value().toObject();
				for( auto app = applications.constBegin(); app != applications.constEnd(); ++app )
				{
					const auto key = record.name + QLatin1Char('|') + user.key() + QLatin1Char('|') + app.key();
					auto& usage = totals[key];
					usage.computer = record.name;
					usage.user = user.key();
					usage.application = app.key();
					usage.seconds += app.value().toInt();
				}
			}
		}
	}
	return totals.values();
}



QString Reports::duration( qint64 seconds )
{
	const auto hours = seconds / 3600;
	const auto minutes = ( seconds % 3600 ) / 60;
	if( hours > 0 )
	{
		return tr( "%1 h %2 min" ).arg( hours ).arg( minutes );
	}
	return tr( "%1 min" ).arg( qMax<qint64>( minutes, seconds > 0 ? 1 : 0 ) );
}



bool Reports::exportAttendanceCsv( const QString& fileName, const QList<Session>& sessions )
{
	QList<QStringList> rows{ { tr( "Date" ), tr( "User" ), tr( "Computer" ), tr( "Logon" ), tr( "Logoff" ), tr( "Minutes" ) } };
	for( const auto& session : sessions )
	{
		const auto start = session.start.toLocalTime();
		rows.append( { start.date().toString( Qt::ISODate ), session.user, session.computer,
					   start.time().toString( QStringLiteral("HH:mm") ),
					   session.end.isValid() ? session.end.toLocalTime().toString( QStringLiteral("HH:mm") )
											 : ( session.open ? tr( "still logged on" ) : tr( "unknown" ) ),
					   session.end.isValid() ? QString::number( session.seconds() / 60 ) : QString{} } );
	}
	return writeReportCsv( fileName, rows );
}



bool Reports::exportUsageCsv( const QString& fileName, const QList<Usage>& usage )
{
	QList<QStringList> rows{ { tr( "Computer" ), tr( "User" ), tr( "Application" ), tr( "Minutes" ) } };
	for( const auto& entry : usage )
	{
		rows.append( { entry.computer, entry.user, entry.application, QString::number( ( entry.seconds + 59 ) / 60 ) } );
	}
	return writeReportCsv( fileName, rows );
}



QString Reports::dailyReport( const QDate& date )
{
	const auto state = GatewayState::load();
	QStringList lines;
	lines.append( tr( "📊 Daily report %1" ).arg( QLocale().toString( date, QLocale::LongFormat ) ) );

	// computers
	const auto records = InventoryStore::list();
	int seenToday = 0;
	QStringList lowDisk;
	QStringList offline;
	const auto now = QDateTime::currentDateTimeUtc();
	for( const auto& record : records )
	{
		if( record.lastSeen.isValid() && record.lastSeen.toLocalTime().date() == date )
		{
			++seenToday;
		}
		const auto free = record.lowestFreePercent();
		if( state.diskAlertPercent > 0 && free >= 0 && free < state.diskAlertPercent )
		{
			lowDisk.append( QStringLiteral("%1 (%2%)").arg( record.name ).arg( free ) );
		}
		if( state.inventoryOfflineDays > 0 && record.lastSeen.isValid() && record.lastSeen.daysTo( now ) >= state.inventoryOfflineDays )
		{
			offline.append( record.name );
		}
	}
	if( records.isEmpty() == false )
	{
		lines.append( tr( "🖥️ Computers online today: %1 of %2" ).arg( seenToday ).arg( records.size() ) );
	}
	if( lowDisk.isEmpty() == false )
	{
		lines.append( tr( "💾 Disk almost full: %1" ).arg( lowDisk.join( QStringLiteral(", ") ) ) );
	}
	if( offline.isEmpty() == false )
	{
		lines.append( tr( "📴 Not online for %1+ days: %2" ).arg( state.inventoryOfflineDays ).arg( offline.join( QStringLiteral(", ") ) ) );
	}

	// schedules that ran today
	const auto schedules = Schedules::load();
	const auto status = Schedules::readStatus();
	QStringList ran;
	for( const auto& rule : schedules.rules )
	{
		const auto entry = status[rule.id].toObject();
		const auto time = QDateTime::fromString( entry[QStringLiteral("time")].toString(), Qt::ISODate );
		if( time.isValid() && time.date() == date )
		{
			ran.append( QStringLiteral("%1 %2 (%3)").arg( time.toString( QStringLiteral("HH:mm") ), rule.name,
														  entry[QStringLiteral("result")].toString() ) );
		}
	}
	if( ran.isEmpty() == false )
	{
		lines.append( tr( "⏰ Schedules:" ) );
		for( const auto& line : std::as_const( ran ) )
		{
			lines.append( QStringLiteral("  • ") + line );
		}
	}

	// attendance
	const auto sessions = attendance( date, date );
	if( sessions.isEmpty() == false )
	{
		QSet<QString> users;
		for( const auto& session : sessions )
		{
			users.insert( session.user );
		}
		lines.append( tr( "👥 Attendance: %1 users, %2 sessions" ).arg( users.size() ).arg( sessions.size() ) );
	}

	// refused accesses and functions
	int refused = 0;
	const auto entries = ActivityLog::read( 100000, QDateTime( date, QTime( 0, 0 ) ).toUTC() );
	for( const auto& entry : entries )
	{
		if( entry.time.toLocalTime().date() == date &&
			( entry.event == QStringLiteral("access.access_denied") || entry.event == QStringLiteral("access.auth_failed") ||
			  entry.event == QStringLiteral("access.feature_denied") || entry.event.endsWith( QStringLiteral(".rejected") ) ) )
		{
			++refused;
		}
	}
	if( refused > 0 )
	{
		lines.append( tr( "🚫 Refused accesses/functions: %1" ).arg( refused ) );
	}

	// most used applications
	QMap<QString, int> applications;
	const auto usageToday = usage( date, date );
	for( const auto& entry : usageToday )
	{
		applications[entry.application] += entry.seconds;
	}
	if( applications.isEmpty() == false )
	{
		QList<QPair<int, QString>> sorted;
		for( auto it = applications.cbegin(); it != applications.cend(); ++it )
		{
			sorted.append( { it.value(), it.key() } );
		}
		std::sort( sorted.begin(), sorted.end(), []( const QPair<int, QString>& a, const QPair<int, QString>& b ) { return a.first > b.first; } );
		QStringList top;
		for( int i = 0; i < qMin( 5, int( sorted.size() ) ); ++i )
		{
			top.append( QStringLiteral("%1 %2").arg( sorted.at( i ).second, duration( sorted.at( i ).first ) ) );
		}
		lines.append( tr( "💻 Most used: %1" ).arg( top.join( QStringLiteral(", ") ) ) );
	}

	if( lines.size() == 1 )
	{
		lines.append( tr( "No activity recorded." ) );
	}
	return lines.join( QLatin1Char('\n') );
}



Reports::DailyReportSettings Reports::loadDailyReportSettings()
{
	DailyReportSettings settings;
	QFile file( dailyReportPath() );
	if( file.open( QFile::ReadOnly ) )
	{
		const auto json = QJsonDocument::fromJson( file.readAll() ).object();
		settings.enabled = json[QStringLiteral("enabled")].toBool();
		const auto time = QTime::fromString( json[QStringLiteral("time")].toString(), QStringLiteral("HH:mm") );
		if( time.isValid() )
		{
			settings.time = time;
		}
		settings.lastSent = json[QStringLiteral("lastSent")].toString();
	}
	return settings;
}



void Reports::saveDailyReportSettings( const DailyReportSettings& settings )
{
	QDir().mkpath( GatewayState::directory() );
	QFile file( dailyReportPath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( QJsonObject{
			{ QStringLiteral("enabled"), settings.enabled },
			{ QStringLiteral("time"), settings.time.toString( QStringLiteral("HH:mm") ) },
			{ QStringLiteral("lastSent"), settings.lastSent },
		} ).toJson() );
	}
}



void Reports::requestDailyReport()
{
	QFile file( dailyReportRequestPath() );
	if( file.open( QFile::WriteOnly ) )
	{
		file.write( "1" );
	}
}



bool Reports::takeDailyReportRequest()
{
	return QFile::remove( dailyReportRequestPath() );
}
