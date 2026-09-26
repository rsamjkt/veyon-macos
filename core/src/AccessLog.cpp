/*
 * AccessLog.cpp - who accessed this computer, when and how
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

#include "AccessLog.h"
#include "Filesystem.h"


QMutex AccessLog::s_mutex;


QString AccessLog::logPath()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/logs/access.jsonl") );
}



void AccessLog::append( const QString& event, const QString& host, const QString& user, const QJsonObject& details )
{
	QMutexLocker locker( &s_mutex );

	const auto path = logPath();
	QDir().mkpath( QFileInfo( path ).absolutePath() );

	if( QFileInfo( path ).size() > MaxFileSize )
	{
		QFile::remove( path + QStringLiteral(".1") );
		QFile::rename( path, path + QStringLiteral(".1") );
	}

	QFile file( path );
	if( file.open( QFile::WriteOnly | QFile::Append ) == false )
	{
		return;
	}

	auto json = details;
	json[QStringLiteral("t")] = QDateTime::currentDateTimeUtc().toString( Qt::ISODate );
	json[QStringLiteral("e")] = event;
	// "::ffff:192.168.1.5" (dual-stack socket) -> "192.168.1.5"
	json[QStringLiteral("host")] = host.startsWith( QStringLiteral("::ffff:") ) ? host.mid( 7 ) : host;
	json[QStringLiteral("user")] = user;
	file.write( QJsonDocument( json ).toJson( QJsonDocument::Compact ) + '\n' );
}



QJsonArray AccessLog::read( const QDateTime& since, int limit )
{
	QMutexLocker locker( &s_mutex );

	QList<QJsonObject> entries;
	for( const auto& path : QStringList{ logPath() + QStringLiteral(".1"), logPath() } )
	{
		QFile file( path );
		if( file.open( QFile::ReadOnly ) == false )
		{
			continue;
		}
		while( file.atEnd() == false )
		{
			const auto json = QJsonDocument::fromJson( file.readLine() ).object();
			if( json.isEmpty() )
			{
				continue;
			}
			if( since.isValid() &&
				QDateTime::fromString( json[QStringLiteral("t")].toString(), Qt::ISODate ) <= since )
			{
				continue;
			}
			entries.append( json );
		}
	}

	QJsonArray result;
	for( auto i = qMax<qsizetype>( 0, entries.size() - limit ); i < entries.size(); ++i )
	{
		result.append( entries.at( i ) );
	}
	return result;
}
