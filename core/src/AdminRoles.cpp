/*
 * AdminRoles.cpp - limited rights for the keys of teachers and admins
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

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QNetworkInterface>

#include "AdminRoles.h"
#include "Filesystem.h"
#include "VeyonCore.h"


namespace {

QMutex rolesMutex;
QList<AdminRoles::Role> cachedRoles;
QDateTime cachedModified;
bool cacheValid = false;

const QHash<QString, QString>& featurePermissions()
{
	static const QHash<QString, QString> map{
		// lock screens, messages, voice broadcast
		{ QStringLiteral("ccb535a2-1d24-4cc1-a709-8b47d2b2ac79"), QStringLiteral("lock") },
		{ QStringLiteral("e4a77879-e544-4fec-bc18-e534f33b934c"), QStringLiteral("lock") },
		{ QStringLiteral("e75ae9c8-ac17-4d00-8f0d-019348346208"), QStringLiteral("lock") },
		{ QStringLiteral("6d3f8a52-e17b-4c90-a4d8-0b95c2e7f316"), QStringLiteral("lock") },
		// internet, websites, exam mode, sound
		{ QStringLiteral("6f3a1d27-9b40-4c85-a216-0e8d5f7b3c91"), QStringLiteral("restrict") },
		{ QStringLiteral("c3a5e2d1-7b4f-4e8a-9d61-2f0b8e7c4a15"), QStringLiteral("restrict") },
		{ QStringLiteral("9b2e6c41-8d7a-4f35-a0c9-4e1b7d3f5a28"), QStringLiteral("restrict") },
		{ QStringLiteral("1a7c5e93-0b62-4d18-9a4f-6e3d8c2b7f50"), QStringLiteral("restrict") },
		// start applications/websites, files
		{ QStringLiteral("da9ca56a-b2ad-4fff-8f8a-929b2927b442"), QStringLiteral("apps") },
		{ QStringLiteral("8a11a75d-b3db-48b6-b9cb-f8422ddd5b0c"), QStringLiteral("apps") },
		{ QStringLiteral("4a70bd5a-fab2-4a4b-a92a-a1e81d2b75ed"), QStringLiteral("apps") },
		{ QStringLiteral("5a14c971-e93c-457f-97a0-0b8f1058a58e"), QStringLiteral("apps") },
		// power and sessions
		{ QStringLiteral("f483c659-b5e7-4dbc-bd91-2c9403e70ebd"), QStringLiteral("power") },
		{ QStringLiteral("4f7d98f0-395a-4fff-b968-e49b8d0f748c"), QStringLiteral("power") },
		{ QStringLiteral("6f5a27a0-0e2f-496e-afcc-7aae62eede10"), QStringLiteral("power") },
		{ QStringLiteral("a88039f2-6716-40d8-b4e1-9f5cd48e91ed"), QStringLiteral("power") },
		{ QStringLiteral("09bcb3a1-fc11-4d03-8cf1-efd26be8655b"), QStringLiteral("power") },
		{ QStringLiteral("ea2406be-d5c7-42b8-9f04-53469d3cc34c"), QStringLiteral("power") },
		{ QStringLiteral("352de795-7fc4-4850-bc57-525bcb7033f5"), QStringLiteral("power") },
		{ QStringLiteral("7310707d-3918-460d-a949-65bd152cb958"), QStringLiteral("power") },
		{ QStringLiteral("7311d43d-ab53-439e-a03a-8cb25f7ed526"), QStringLiteral("power") },
		// installing and removing software
		{ QStringLiteral("7e4c2a19-3b8d-4f61-9a05-c6d2e8f14b37"), QStringLiteral("software") },
		// the role policy itself - only keys with full rights
		{ QStringLiteral("2f8b6d14-9c3e-4a57-b0e1-5d7a9c4f8e23"), QStringLiteral("admin") },
	};
	return map;
}



QString uidKey( Feature::Uid uid )
{
	return uid.toString( QUuid::WithoutBraces ).toLower();
}



bool isLocalName( const QString& host )
{
	const auto local = QHostInfo::localHostName().toLower();
	const auto name = host.trimmed().toLower();
	if( name.isEmpty() )
	{
		return false;
	}
	// "pc-01" matches "pc-01.local" / "pc-01.school.lan" and the other way round
	return name == local || name.section( QLatin1Char('.'), 0, 0 ) == local.section( QLatin1Char('.'), 0, 0 );
}

}



QStringList AdminRoles::permissions()
{
	return { QStringLiteral("lock"), QStringLiteral("restrict"), QStringLiteral("apps"),
			 QStringLiteral("power"), QStringLiteral("software") };
}



QString AdminRoles::permissionName( const QString& permission )
{
	if( permission == QStringLiteral("lock") ) return tr( "Lock screens, messages, broadcast" );
	if( permission == QStringLiteral("restrict") ) return tr( "Block internet/websites, exam mode, mute" );
	if( permission == QStringLiteral("apps") ) return tr( "Open applications/websites, send files" );
	if( permission == QStringLiteral("power") ) return tr( "Power on/off, reboot, log off" );
	if( permission == QStringLiteral("software") ) return tr( "Install and remove software" );
	if( permission == QStringLiteral("admin") ) return tr( "Change the roles" );
	return permission;
}



QString AdminRoles::permissionOf( Feature::Uid featureUid )
{
	return featurePermissions().value( uidKey( featureUid ) );
}



QString AdminRoles::path()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/roles.json") );
}



QByteArray AdminRoles::toJson( const QList<Role>& roles )
{
	QJsonArray array;
	for( const auto& role : roles )
	{
		array.append( QJsonObject{
			{ QStringLiteral("key"), role.key },
			{ QStringLiteral("name"), role.name },
			{ QStringLiteral("public"), QString::fromLatin1( role.publicPem ) },
			{ QStringLiteral("rooms"), QJsonArray::fromStringList( role.rooms ) },
			{ QStringLiteral("hosts"), QJsonArray::fromStringList( role.hosts ) },
			{ QStringLiteral("allowed"), QJsonArray::fromStringList( role.allowed ) },
		} );
	}
	return QJsonDocument( QJsonObject{ { QStringLiteral("v"), 1 }, { QStringLiteral("roles"), array } } ).toJson();
}



QList<AdminRoles::Role> AdminRoles::fromJson( const QByteArray& json )
{
	QList<Role> roles;
	const auto array = QJsonDocument::fromJson( json ).object()[QStringLiteral("roles")].toArray();
	for( const auto& value : array )
	{
		const auto object = value.toObject();
		Role role;
		role.key = object[QStringLiteral("key")].toString();
		role.name = object[QStringLiteral("name")].toString();
		role.publicPem = object[QStringLiteral("public")].toString().toLatin1();
		role.rooms = object[QStringLiteral("rooms")].toVariant().toStringList();
		role.hosts = object[QStringLiteral("hosts")].toVariant().toStringList();
		role.allowed = object[QStringLiteral("allowed")].toVariant().toStringList();
		role.allowed.removeAll( QStringLiteral("admin") );
		if( VeyonCore::isAuthenticationKeyNameValid( role.key ) )
		{
			roles.append( role );
		}
	}
	return roles;
}



QList<AdminRoles::Role> AdminRoles::load()
{
	QMutexLocker locker( &rolesMutex );

	const QFileInfo info( path() );
	const auto modified = info.exists() ? info.lastModified() : QDateTime{};
	if( cacheValid && modified == cachedModified )
	{
		return cachedRoles;
	}

	cachedRoles.clear();
	QFile file( path() );
	if( file.open( QFile::ReadOnly ) )
	{
		cachedRoles = fromJson( file.readAll() );
	}
	cachedModified = modified;
	cacheValid = true;
	return cachedRoles;
}



bool AdminRoles::save( const QList<Role>& roles )
{
	QDir().mkpath( QFileInfo( path() ).absolutePath() );
	const auto tempPath = path() + QStringLiteral(".tmp");
	QFile file( tempPath );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return false;
	}
	const auto json = toJson( roles );
	if( file.write( json ) != json.size() )
	{
		return false;
	}
	file.close();
	QFile::remove( path() );
	const bool ok = QFile::rename( tempPath, path() );

	QMutexLocker locker( &rolesMutex );
	cacheValid = false;
	return ok;
}



AdminRoles::Role AdminRoles::roleForKey( const QString& keyName )
{
	if( keyName.isEmpty() )
	{
		return {};
	}
	const auto roles = load();
	for( const auto& role : roles )
	{
		if( role.key == keyName )
		{
			return role;
		}
	}
	return {};
}



bool AdminRoles::isComputerAllowed( const QString& keyName )
{
	const auto role = roleForKey( keyName );
	if( role.isValid() == false || role.hosts.isEmpty() )
	{
		return true;
	}

	const auto addresses = QNetworkInterface::allAddresses();
	for( const auto& host : role.hosts )
	{
		if( isLocalName( host ) )
		{
			return true;
		}
		const QHostAddress address( host );
		if( address.isNull() == false &&
			std::any_of( addresses.cbegin(), addresses.cend(), [&address]( const QHostAddress& local ) {
				return local.isEqual( address, QHostAddress::ConvertV4MappedToIPv4 );
			} ) )
		{
			return true;
		}
	}
	return false;
}



bool AdminRoles::isFeatureAllowed( const QString& keyName, Feature::Uid featureUid )
{
	const auto permission = permissionOf( featureUid );
	if( permission.isEmpty() )
	{
		return true;
	}

	const auto role = roleForKey( keyName );
	if( role.isValid() == false )
	{
		return true;
	}

	return role.allowed.contains( permission );
}
