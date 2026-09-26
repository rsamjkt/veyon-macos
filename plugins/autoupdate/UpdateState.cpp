/*
 * UpdateState.cpp - settings and status of the automatic updates
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

#include "Filesystem.h"
#include "UpdateState.h"
#include "VeyonCore.h"


namespace {

QString path( const QString& fileName )
{
	return QDir( UpdateState::directory() ).filePath( fileName );
}



QJsonObject readJson( const QString& fileName )
{
	QFile file( path( fileName ) );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	return QJsonDocument::fromJson( file.readAll() ).object();
}



bool writeJson( const QString& fileName, const QJsonObject& json )
{
	QDir().mkpath( UpdateState::directory() );
	const auto target = path( fileName );
	const auto temp = target + QStringLiteral(".tmp");
	QFile file( temp );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return false;
	}
	file.write( QJsonDocument( json ).toJson() );
	file.close();
	QFile::remove( target );
	return QFile::rename( temp, target );
}

}



QString UpdateState::directory()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/update") );
}



UpdateState UpdateState::load()
{
	UpdateState state;
	const auto json = readJson( QStringLiteral("settings.json") );
	state.enabled = json[QStringLiteral("enabled")].toBool( true );
	const auto url = json[QStringLiteral("url")].toString();
	if( url.startsWith( QStringLiteral("https://") ) )
	{
		state.manifestUrl = url;
	}
	return state;
}



bool UpdateState::save() const
{
	return writeJson( QStringLiteral("settings.json"), {
		{ QStringLiteral("enabled"), enabled },
		{ QStringLiteral("url"), manifestUrl },
	} );
}



QJsonObject UpdateState::readStatus()
{
	return readJson( QStringLiteral("status.json") );
}



void UpdateState::writeStatus( const QJsonObject& status )
{
	writeJson( QStringLiteral("status.json"), status );
}



void UpdateState::requestCheck()
{
	QDir().mkpath( directory() );
	QFile file( path( QStringLiteral("check-now") ) );
	if( file.open( QFile::WriteOnly ) )
	{
		file.close();
	}
}



bool UpdateState::takeCheckRequest()
{
	return QFile::remove( path( QStringLiteral("check-now") ) );
}
