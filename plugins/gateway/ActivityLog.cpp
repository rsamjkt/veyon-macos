/*
 * ActivityLog.cpp - history of the roaming laptops and phones of a gateway
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
#include <QJsonDocument>
#include <QLockFile>
#include <QTextStream>

#include "ActivityLog.h"
#include "GatewayState.h"


namespace {

QString rotatedPath()
{
	return ActivityLog::logPath() + QStringLiteral(".1");
}



QString csvField( QString value )
{
	if( value.contains( QLatin1Char(';') ) || value.contains( QLatin1Char('"') ) || value.contains( QLatin1Char('\n') ) )
	{
		value.replace( QLatin1Char('"'), QStringLiteral("\"\"") );
		return QLatin1Char('"') + value + QLatin1Char('"');
	}
	return value;
}



QString duration( qint64 seconds )
{
	if( seconds < 3600 )
	{
		return ActivityLog::tr( "%1 min" ).arg( qMax<qint64>( 1, seconds / 60 ) );
	}
	if( seconds < 2 * 86400 )
	{
		return ActivityLog::tr( "%1 h %2 min" ).arg( seconds / 3600 ).arg( ( seconds % 3600 ) / 60 );
	}
	return ActivityLog::tr( "%1 days" ).arg( seconds / 86400 );
}

}



QString ActivityLog::logPath()
{
	return QDir( GatewayState::directory() ).filePath( QStringLiteral("activity.jsonl") );
}



void ActivityLog::append( const QString& event, const QString& subject, const QJsonObject& details )
{
	appendAt( QDateTime::currentDateTimeUtc(), event, subject, details );
}



void ActivityLog::appendAt( const QDateTime& time, const QString& event, const QString& subject, const QJsonObject& details )
{
	QDir().mkpath( GatewayState::directory() );

	QLockFile lock( logPath() + QStringLiteral(".lock") );
	lock.setStaleLockTime( 5000 );
	if( lock.tryLock( 2000 ) == false )
	{
		return;
	}

	// keep one rotated file - two files of MaxFileSize hold months of history
	if( QFileInfo( logPath() ).size() > MaxFileSize )
	{
		QFile::remove( rotatedPath() );
		QFile::rename( logPath(), rotatedPath() );
	}

	QFile file( logPath() );
	if( file.open( QFile::WriteOnly | QFile::Append ) == false )
	{
		return;
	}

	auto json = details;
	json[QStringLiteral("t")] = time.toUTC().toString( Qt::ISODate );
	json[QStringLiteral("e")] = event;
	json[QStringLiteral("s")] = subject;
	file.write( QJsonDocument( json ).toJson( QJsonDocument::Compact ) + '\n' );
}



QList<ActivityLog::Entry> ActivityLog::read( int maximum, const QDateTime& since )
{
	const auto oldest = since.isValid() ? since : QDateTime::currentDateTimeUtc().addDays( -RetentionDays );

	QList<Entry> entries;
	for( const auto& path : { logPath(), rotatedPath() } )
	{
		QFile file( path );
		if( file.open( QFile::ReadOnly ) == false )
		{
			continue;
		}

		QList<Entry> fileEntries;
		while( file.atEnd() == false )
		{
			const auto json = QJsonDocument::fromJson( file.readLine() ).object();
			if( json.isEmpty() )
			{
				continue;
			}

			Entry entry;
			entry.time = QDateTime::fromString( json[QStringLiteral("t")].toString(), Qt::ISODate );
			if( entry.time.isValid() == false || entry.time < oldest )
			{
				continue;
			}
			entry.event = json[QStringLiteral("e")].toString();
			entry.subject = json[QStringLiteral("s")].toString();
			entry.details = json;
			entry.details.remove( QStringLiteral("t") );
			entry.details.remove( QStringLiteral("e") );
			entry.details.remove( QStringLiteral("s") );
			fileEntries.append( entry );
		}

		entries.append( fileEntries );
	}

	// collected access logs are appended later than they happened - order by
	// the time of the event, newest first
	std::stable_sort( entries.begin(), entries.end(), []( const Entry& a, const Entry& b ) { return a.time > b.time; } );
	if( entries.size() > maximum )
	{
		entries.resize( maximum );
	}
	return entries;
}



bool ActivityLog::exportCsv( const QString& fileName, const QDateTime& since )
{
	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return false;
	}

	QTextStream stream( &file );
	// UTF-8 BOM and ';' make Excel with Indonesian regional settings open it
	// correctly with a double-click
	stream << QChar( 0xFEFF );
	stream << tr( "Time" ) << ';' << tr( "Event" ) << ';' << tr( "Laptop / phone" ) << ';'
		   << tr( "User" ) << ';' << tr( "Details" ) << '\n';

	const auto entries = read( std::numeric_limits<int>::max(), since );
	for( auto it = entries.crbegin(); it != entries.crend(); ++it )
	{
		stream << it->time.toLocalTime().toString( QStringLiteral("yyyy-MM-dd HH:mm:ss") ) << ';'
			   << csvField( eventName( it->event ) ) << ';'
			   << csvField( it->subject ) << ';'
			   << csvField( it->details[QStringLiteral("user")].toString() ) << ';'
			   << csvField( describe( *it ) ) << '\n';
	}

	return stream.status() == QTextStream::Ok;
}



QString ActivityLog::eventName( const QString& event )
{
	static const QHash<QString, const char*> names{
		{ QStringLiteral("laptop.online"), QT_TR_NOOP( "Laptop online" ) },
		{ QStringLiteral("laptop.offline"), QT_TR_NOOP( "Laptop offline" ) },
		{ QStringLiteral("laptop.location"), QT_TR_NOOP( "Location changed" ) },
		{ QStringLiteral("laptop.user"), QT_TR_NOOP( "User changed" ) },
		{ QStringLiteral("laptop.app"), QT_TR_NOOP( "Application" ) },
		{ QStringLiteral("laptop.registered"), QT_TR_NOOP( "Laptop registered" ) },
		{ QStringLiteral("laptop.removed"), QT_TR_NOOP( "Laptop removed" ) },
		{ QStringLiteral("laptop.rejected"), QT_TR_NOOP( "Laptop refused" ) },
		{ QStringLiteral("laptop.screenshot"), QT_TR_NOOP( "Screenshot" ) },
		{ QStringLiteral("phone.connected"), QT_TR_NOOP( "Phone connected" ) },
		{ QStringLiteral("phone.paired"), QT_TR_NOOP( "Phone paired" ) },
		{ QStringLiteral("phone.rejected"), QT_TR_NOOP( "Access refused" ) },
		{ QStringLiteral("access.connected"), QT_TR_NOOP( "Access: connected" ) },
		{ QStringLiteral("access.disconnected"), QT_TR_NOOP( "Access: disconnected" ) },
		{ QStringLiteral("access.auth_failed"), QT_TR_NOOP( "Access: authentication failed" ) },
		{ QStringLiteral("access.access_denied"), QT_TR_NOOP( "Access: denied" ) },
		{ QStringLiteral("access.feature"), QT_TR_NOOP( "Access: function used" ) },
	};
	const auto name = names.value( event );
	return name ? tr( name ) : event;
}



QString ActivityLog::describe( const Entry& entry )
{
	const auto& d = entry.details;
	const auto where = d[QStringLiteral("where")].toString() == QStringLiteral("office") ? tr( "in the office" )
																						: tr( "outside the office" );

	if( entry.event == QStringLiteral("laptop.online") )
	{
		return tr( "Online %1, user %2" ).arg( where, d[QStringLiteral("user")].toString( tr( "none" ) ) );
	}
	if( entry.event == QStringLiteral("laptop.offline") )
	{
		return tr( "Offline after %1" ).arg( duration( d[QStringLiteral("seconds")].toInteger() ) );
	}
	if( entry.event == QStringLiteral("laptop.location") )
	{
		return tr( "Now %1" ).arg( where );
	}
	if( entry.event == QStringLiteral("laptop.user") )
	{
		const auto user = d[QStringLiteral("user")].toString();
		return user.isEmpty() ? tr( "User logged off" ) : tr( "%1 logged on" ).arg( user );
	}
	if( entry.event == QStringLiteral("laptop.app") )
	{
		return tr( "Active application: %1" ).arg( d[QStringLiteral("app")].toString() );
	}
	if( entry.event == QStringLiteral("laptop.screenshot") )
	{
		return d[QStringLiteral("file")].toString();
	}
	if( entry.event.startsWith( QStringLiteral("access.") ) )
	{
		auto text = tr( "from %1 (%2)" ).arg( d[QStringLiteral("host")].toString(), d[QStringLiteral("user")].toString() );
		if( entry.event == QStringLiteral("access.feature") )
		{
			text.prepend( d[QStringLiteral("feature")].toString() + QLatin1Char(' ') );
		}
		else if( entry.event == QStringLiteral("access.disconnected") )
		{
			text += QStringLiteral(", ") + tr( "after %1" ).arg( duration( d[QStringLiteral("seconds")].toInteger() ) );
		}
		return text;
	}
	if( entry.event == QStringLiteral("phone.rejected") || entry.event == QStringLiteral("laptop.rejected") )
	{
		return tr( "Unknown device or invalid code" );
	}
	return eventName( entry.event );
}
