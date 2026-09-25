/*
 * GatewayState.cpp - persistent state of the Aruni Gateway
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
#include <QHostInfo>
#include <QJsonDocument>
#include <QLockFile>

#include "Filesystem.h"
#include "GatewayState.h"
#include "PlatformFilesystemFunctions.h"
#include "VeyonCore.h"


namespace {

QString readableId( const QByteArray& random )
{
	// 16 characters, unambiguous alphabet (no 0/O, 1/I/L)
	static const char alphabet[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
	QString id;
	for( const auto byte : random )
	{
		id += QLatin1Char( alphabet[quint8( byte ) % ( sizeof(alphabet) - 1 )] );
	}
	return id;
}

}



QString GatewayState::directory()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/gateway") );
}



QString GatewayState::statePath()
{
	return QDir( directory() ).filePath( QStringLiteral("gateway.json") );
}



QString GatewayState::statusPath()
{
	return QDir( directory() ).filePath( QStringLiteral("status.json") );
}



GatewayState GatewayState::load()
{
	GatewayState state;

	QFile file( statePath() );
	if( file.open( QFile::ReadOnly ) )
	{
		const auto json = QJsonDocument::fromJson( file.readAll() ).object();
		state.enabled = json[QStringLiteral("enabled")].toBool();
		state.relayUrl = json[QStringLiteral("relayUrl")].toString( QString::fromLatin1( DefaultRelayUrl ) );
		state.siteName = json[QStringLiteral("siteName")].toString();
		state.gatewayId = json[QStringLiteral("id")].toString();
		state.relaySecret = json[QStringLiteral("secret")].toString();
		state.keyPair = AruniTunnel::KeyPair::fromPrivateKey( AruniTunnel::fromBase64Url( json[QStringLiteral("key")].toString() ) );
		state.pairingToken = AruniTunnel::fromBase64Url( json[QStringLiteral("pairingToken")].toString() );
		state.pairingExpires = QDateTime::fromString( json[QStringLiteral("pairingExpires")].toString(), Qt::ISODate );
		state.sharedKeyName = json[QStringLiteral("sharedKey")].toString();

		for( const auto& value : json[QStringLiteral("devices")].toArray() )
		{
			const auto object = value.toObject();
			Device device;
			device.publicKey = AruniTunnel::fromBase64Url( object[QStringLiteral("key")].toString() );
			device.name = object[QStringLiteral("name")].toString();
			device.added = QDateTime::fromString( object[QStringLiteral("added")].toString(), Qt::ISODate );
			device.lastSeen = QDateTime::fromString( object[QStringLiteral("lastSeen")].toString(), Qt::ISODate );
			if( device.publicKey.size() == AruniTunnel::KeySize )
			{
				state.devices.append( device );
			}
		}
	}

	if( state.siteName.isEmpty() )
	{
		state.siteName = QHostInfo::localHostName();
	}

	// identity is created lazily on first use
	if( state.gatewayId.isEmpty() || state.relaySecret.size() < 32 || state.keyPair.isValid() == false )
	{
		state.gatewayId = readableId( AruniTunnel::randomBytes( 16 ) );
		state.relaySecret = AruniTunnel::toBase64Url( AruniTunnel::randomBytes( 32 ) );
		state.keyPair = AruniTunnel::KeyPair::generate();
		state.devices.clear();
	}

	return state;
}



bool GatewayState::save() const
{
	QJsonArray deviceArray;
	for( const auto& device : devices )
	{
		deviceArray.append( QJsonObject{
			{ QStringLiteral("key"), AruniTunnel::toBase64Url( device.publicKey ) },
			{ QStringLiteral("name"), device.name },
			{ QStringLiteral("added"), device.added.toString( Qt::ISODate ) },
			{ QStringLiteral("lastSeen"), device.lastSeen.toString( Qt::ISODate ) },
		} );
	}

	const QJsonObject json{
		{ QStringLiteral("enabled"), enabled },
		{ QStringLiteral("relayUrl"), relayUrl },
		{ QStringLiteral("siteName"), siteName },
		{ QStringLiteral("id"), gatewayId },
		{ QStringLiteral("secret"), relaySecret },
		{ QStringLiteral("key"), AruniTunnel::toBase64Url( keyPair.privateKey ) },
		{ QStringLiteral("pairingToken"), AruniTunnel::toBase64Url( pairingToken ) },
		{ QStringLiteral("pairingExpires"), pairingExpires.toString( Qt::ISODate ) },
		{ QStringLiteral("sharedKey"), sharedKeyName },
		{ QStringLiteral("devices"), deviceArray },
	};

	QDir().mkpath( directory() );

	// the file holds the gateway's private key - only its owner (the service /
	// administrators) may read it
	const auto tempPath = statePath() + QStringLiteral(".tmp");
	QFile::remove( tempPath );
	QFile file( tempPath );
	if( VeyonCore::platform().filesystemFunctions().openFileSafely( &file, QFile::WriteOnly | QFile::Truncate,
																	 QFile::ReadOwner | QFile::WriteOwner ) == false )
	{
		vCritical() << "cannot write" << tempPath;
		return false;
	}
	file.write( QJsonDocument( json ).toJson() );
	file.close();

	QFile::remove( statePath() );
	return QFile::rename( tempPath, statePath() );
}



bool GatewayState::update( const std::function<void(GatewayState&)>& modifier )
{
	QDir().mkpath( directory() );
	QLockFile lock( statePath() + QStringLiteral(".lock") );
	lock.setStaleLockTime( 10000 );
	if( lock.tryLock( 5000 ) == false )
	{
		vWarning() << "gateway state is locked";
		return false;
	}

	auto state = load();
	modifier( state );
	return state.save();
}



AruniTunnel::PairingInfo GatewayState::pairingInfo() const
{
	AruniTunnel::PairingInfo info;
	info.relayUrl = relayUrl;
	info.gatewayId = gatewayId;
	info.siteName = siteName;
	info.gatewayPublicKey = keyPair.publicKey;
	info.token = pairingToken;
	return info;
}



void GatewayState::writeStatus( const QJsonObject& status )
{
	QDir().mkpath( directory() );
	QFile file( statusPath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( status ).toJson( QJsonDocument::Compact ) );
	}
}



QJsonObject GatewayState::readStatus()
{
	QFile file( statusPath() );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	return QJsonDocument::fromJson( file.readAll() ).object();
}
