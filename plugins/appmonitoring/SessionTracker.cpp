/*
 * SessionTracker.cpp - attendance and application usage of this computer
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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>

#include "AccessLog.h"
#include "ApplicationList.h"
#include "Filesystem.h"
#include "PlatformSessionFunctions.h"
#include "PlatformUserFunctions.h"
#include "SessionTracker.h"
#include "VeyonCore.h"


namespace {

QString sessionStatePath()
{
	// one file per session - Windows runs a server per logged-on user
	const auto sessionId = VeyonCore::platform().sessionFunctions().currentSessionId();
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/logs/session-%1.json").arg( sessionId ) );
}

}



SessionTracker::SessionTracker( QObject* parent ) :
	QObject( parent )
{
	finishPreviousSession();

	connect( &m_sampleTimer, &QTimer::timeout, this, &SessionTracker::sample );
	m_sampleTimer.start( SampleInterval * 1000 );
	connect( &m_flushTimer, &QTimer::timeout, this, &SessionTracker::flush );
	m_flushTimer.start( FlushInterval );

	QTimer::singleShot( 2000, this, &SessionTracker::sample );
}



SessionTracker::~SessionTracker()
{
	flush();
	if( m_user.isEmpty() == false )
	{
		AccessLog::append( QStringLiteral("user_logout"), {}, m_user );
		m_user.clear();
		saveState();
	}
}



QString SessionTracker::usageDirectory()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/logs/usage") );
}



QJsonObject SessionTracker::readUsage( int days )
{
	QJsonObject result;
	const QDir dir( usageDirectory() );
	for( int i = 0; i < days; ++i )
	{
		const auto date = QDate::currentDate().addDays( -i ).toString( Qt::ISODate );
		QFile file( dir.filePath( date + QStringLiteral(".json") ) );
		if( file.open( QFile::ReadOnly ) )
		{
			result[date] = QJsonDocument::fromJson( file.readAll() ).object();
		}
	}
	return result;
}



void SessionTracker::finishPreviousSession()
{
	// the computer was switched off or the server was killed during a session
	QFile file( sessionStatePath() );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return;
	}
	const auto state = QJsonDocument::fromJson( file.readAll() ).object();
	file.close();

	const auto user = state[QStringLiteral("user")].toString();
	if( user.isEmpty() == false )
	{
		AccessLog::append( QStringLiteral("user_logout"), {}, user,
						   { { QStringLiteral("at"), state[QStringLiteral("lastActive")].toString() } } );
	}
	QFile::remove( sessionStatePath() );
}



void SessionTracker::sample()
{
	const auto user = VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );

	if( user != m_user )
	{
		if( m_user.isEmpty() == false )
		{
			AccessLog::append( QStringLiteral("user_logout"), {}, m_user );
		}
		if( user.isEmpty() == false )
		{
			AccessLog::append( QStringLiteral("user_login"), {}, user );
		}
		m_user = user;
		m_lastActive = QDateTime::currentDateTimeUtc();
		saveState();
	}

	if( m_user.isEmpty() || secondsSinceLastInput() >= IdleLimit )
	{
		return;
	}

	m_lastActive = QDateTime::currentDateTimeUtc();

	const auto application = frontmostApplication().trimmed();
	if( application.isEmpty() )
	{
		return;
	}

	const auto today = QDate::currentDate();
	if( m_pendingDate.isValid() && m_pendingDate != today )
	{
		flush();
	}
	m_pendingDate = today;
	m_pending[m_user][application] += SampleInterval;
}



void SessionTracker::flush()
{
	saveState();

	if( m_pending.isEmpty() || m_pendingDate.isValid() == false )
	{
		return;
	}

	const QDir dir( usageDirectory() );
	QDir().mkpath( dir.path() );

	// the servers of other sessions write the same day file
	QLockFile lock( dir.filePath( QStringLiteral("usage.lock") ) );
	lock.setStaleLockTime( 10000 );
	if( lock.tryLock( 5000 ) == false )
	{
		return;
	}

	const auto fileName = dir.filePath( m_pendingDate.toString( Qt::ISODate ) + QStringLiteral(".json") );
	QJsonObject usage;
	{
		QFile file( fileName );
		if( file.open( QFile::ReadOnly ) )
		{
			usage = QJsonDocument::fromJson( file.readAll() ).object();
		}
	}

	for( auto user = m_pending.cbegin(); user != m_pending.cend(); ++user )
	{
		auto applications = usage[user.key()].toObject();
		for( auto app = user->cbegin(); app != user->cend(); ++app )
		{
			applications[app.key()] = applications[app.key()].toInt() + app.value();
		}
		usage[user.key()] = applications;
	}

	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( usage ).toJson( QJsonDocument::Compact ) );
		m_pending.clear();
	}

	// keep two months
	const auto oldest = QDate::currentDate().addDays( -KeepDays );
	const auto files = dir.entryList( { QStringLiteral("*.json") }, QDir::Files );
	for( const auto& name : files )
	{
		const auto date = QDate::fromString( QFileInfo( name ).completeBaseName(), Qt::ISODate );
		if( date.isValid() && date < oldest )
		{
			QFile::remove( dir.filePath( name ) );
		}
	}
}



void SessionTracker::saveState()
{
	const auto path = sessionStatePath();
	if( m_user.isEmpty() )
	{
		QFile::remove( path );
		return;
	}
	QDir().mkpath( QFileInfo( path ).absolutePath() );
	QFile file( path );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( QJsonObject{
			{ QStringLiteral("user"), m_user },
			{ QStringLiteral("lastActive"), m_lastActive.toString( Qt::ISODate ) },
		} ).toJson( QJsonDocument::Compact ) );
	}
}
