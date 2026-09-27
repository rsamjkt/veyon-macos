/*
 * SetupCode.cpp - one code that sets up a computer
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
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "AruniTunnel.h"
#include "CryptoCore.h"
#include "Filesystem.h"
#include "GatewayState.h"
#include "NetworkObject.h"
#include "SetupCode.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


namespace {

QByteArray readKeyFile( const QString& fileName )
{
	QFile file( fileName );
	return file.open( QFile::ReadOnly ) ? file.readAll() : QByteArray{};
}



bool writeKeyFile( const QString& fileName, const QByteArray& pem, bool isPrivate )
{
	if( VeyonCore::filesystem().ensurePathExists( QFileInfo( fileName ).path() ) == false )
	{
		return false;
	}

	if( readKeyFile( fileName ) == pem )
	{
		return true;
	}

	QFile::setPermissions( fileName, QFile::ReadOwner | QFile::WriteOwner );
	QFile::remove( fileName );
	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false || file.write( pem ) != pem.size() )
	{
		return false;
	}
	file.close();

	// the same permissions "veyon-cli authkeys import" sets
	return QFile::setPermissions( fileName, isPrivate ? ( QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup )
													  : ( QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther ) );
}

}



QStringList SetupCode::availableKeys()
{
	const auto baseDir = VeyonCore::filesystem().expandPath( VeyonCore::config().publicKeyBaseDir() );
	QStringList names;
	const auto entries = QDir( baseDir ).entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );
	for( const auto& name : entries )
	{
		if( QFileInfo::exists( VeyonCore::filesystem().publicKeyPath( name ) ) )
		{
			names.append( name );
		}
	}
	return names;
}



bool SetupCode::hasPrivateKey( const QString& name )
{
	return QFileInfo::exists( VeyonCore::filesystem().privateKeyPath( name ) );
}



QString SetupCode::create( const Options& options, QString* error )
{
	QJsonArray keys;
	for( const auto& name : options.keyNames )
	{
		const auto publicPem = readKeyFile( VeyonCore::filesystem().publicKeyPath( name ) );
		if( publicPem.isEmpty() )
		{
			if( error )
			{
				*error = tr( "Cannot read the public key \"%1\"." ).arg( name );
			}
			return {};
		}

		QJsonObject key{ { QStringLiteral("name"), name }, { QStringLiteral("public"), QString::fromLatin1( publicPem ) } };
		if( options.includePrivateKeys && hasPrivateKey( name ) )
		{
			const auto privatePem = readKeyFile( VeyonCore::filesystem().privateKeyPath( name ) );
			if( privatePem.isEmpty() )
			{
				if( error )
				{
					*error = tr( "Cannot read the private key \"%1\" (run as administrator)." ).arg( name );
				}
				return {};
			}
			key[QStringLiteral("private")] = QString::fromLatin1( privatePem );
		}
		keys.append( key );
	}

	const auto state = GatewayState::load();

	QJsonObject json{
		{ QStringLiteral("v"), 1 },
		{ QStringLiteral("site"), state.siteName },
		{ QStringLiteral("keys"), keys },
	};

	if( options.includeComputers )
	{
		json[QStringLiteral("computers")] = VeyonCore::config().value( QStringLiteral("NetworkObjects"),
																	   QStringLiteral("BuiltinDirectory"), {} ).toJsonArray();
	}

	if( options.includeRoaming )
	{
		if( state.enabled == false )
		{
			if( error )
			{
				*error = tr( "Roaming laptops need the Aruni Gateway enabled on this computer." );
			}
			return {};
		}
		json[QStringLiteral("enroll")] = state.enrollmentInfo().encode();
	}

	const auto data = qCompress( QJsonDocument( json ).toJson( QJsonDocument::Compact ), 9 );
	return QString::fromLatin1( Prefix ) + AruniTunnel::toBase64Url( data );
}



SetupCode::Content SetupCode::decode( const QString& code )
{
	Content content;

	auto text = code.trimmed();
	text.remove( QLatin1Char('"') );
	text.remove( QRegularExpression( QStringLiteral("\\s") ) );
	if( text.startsWith( QString::fromLatin1( Prefix ), Qt::CaseInsensitive ) == false )
	{
		return content;
	}

	const auto data = qUncompress( AruniTunnel::fromBase64Url( text.mid( int( qstrlen( Prefix ) ) ) ) );
	const auto json = QJsonDocument::fromJson( data ).object();
	if( json[QStringLiteral("v")].toInt() != 1 )
	{
		return content;
	}

	content.siteName = json[QStringLiteral("site")].toString();

	const auto keys = json[QStringLiteral("keys")].toArray();
	for( const auto& value : keys )
	{
		const auto object = value.toObject();
		Key key;
		key.name = object[QStringLiteral("name")].toString();
		key.publicPem = object[QStringLiteral("public")].toString().toLatin1();
		key.privatePem = object[QStringLiteral("private")].toString().toLatin1();
		if( VeyonCore::isAuthenticationKeyNameValid( key.name ) && key.publicPem.isEmpty() == false )
		{
			content.keys.append( key );
		}
	}

	if( json.contains( QStringLiteral("computers") ) )
	{
		content.computers = QJsonDocument( json[QStringLiteral("computers")].toArray() ).toJson( QJsonDocument::Compact );
	}

	const auto enroll = json[QStringLiteral("enroll")].toString();
	const auto hub = AruniTunnel::PairingInfo::decode( enroll );
	if( hub.enrollment && hub.isValid() )
	{
		content.enrollmentCode = enroll;
	}

	return content;
}



QStringList SetupCode::describe( const Content& content )
{
	QStringList lines;
	if( content.siteName.isEmpty() == false )
	{
		lines.append( tr( "From: %1" ).arg( content.siteName ) );
	}
	for( const auto& key : content.keys )
	{
		lines.append( key.privatePem.isEmpty() ? tr( "Authentication key \"%1\" (public)" ).arg( key.name )
											   : tr( "Authentication key \"%1\" (public and private - for Master computers)" ).arg( key.name ) );
	}
	if( content.computers.isEmpty() == false )
	{
		const auto objects = QJsonDocument::fromJson( content.computers ).array();
		int computers = 0;
		for( const auto& object : objects )
		{
			if( object.toObject()[QStringLiteral("Type")].toInt() == int( NetworkObject::Type::Host ) )
			{
				++computers;
			}
		}
		lines.append( tr( "List of rooms and computers (%1 computers)" ).arg( computers ) );
	}
	if( content.enrollmentCode.isEmpty() == false )
	{
		lines.append( tr( "Roaming laptop: stays reachable outside the office" ) );
	}
	return lines;
}



bool SetupCode::apply( const Content& content, QStringList& report, QString& error )
{
	if( content.isValid() == false )
	{
		error = tr( "This is not a valid installation code." );
		return false;
	}

	for( const auto& key : content.keys )
	{
		if( CryptoCore::PublicKey::fromPEM( QString::fromLatin1( key.publicPem ) ).isPublic() == false )
		{
			error = tr( "The key \"%1\" in the code is damaged." ).arg( key.name );
			return false;
		}
		if( writeKeyFile( VeyonCore::filesystem().publicKeyPath( key.name ), key.publicPem, false ) == false )
		{
			error = tr( "Cannot write the key \"%1\" (run as administrator)." ).arg( key.name );
			return false;
		}
		report.append( tr( "Authentication key \"%1\" installed" ).arg( key.name ) );

		if( key.privatePem.isEmpty() == false )
		{
			if( CryptoCore::PrivateKey::fromPEM( QString::fromLatin1( key.privatePem ) ).isPrivate() == false ||
				writeKeyFile( VeyonCore::filesystem().privateKeyPath( key.name ), key.privatePem, true ) == false )
			{
				error = tr( "Cannot write the private key \"%1\" (run as administrator)." ).arg( key.name );
				return false;
			}
			report.append( tr( "Private key \"%1\" installed (this computer can act as Master)" ).arg( key.name ) );
		}
	}

	// one write of the configuration for everything
	auto& config = VeyonCore::config();
	if( content.keys.isEmpty() == false )
	{
		config.setAuthenticationMethod( VeyonCore::AuthenticationMethod::KeyFileAuthentication );
		report.append( tr( "Authentication method: key files" ) );
	}
	if( content.computers.isEmpty() == false )
	{
		config.setValue( QStringLiteral("NetworkObjects"), QJsonDocument::fromJson( content.computers ).array(),
						 QStringLiteral("BuiltinDirectory") );
		report.append( tr( "List of rooms and computers imported" ) );
	}
	if( content.keys.isEmpty() == false || content.computers.isEmpty() == false )
	{
		config.flushStore();
	}

	if( content.enrollmentCode.isEmpty() == false )
	{
		const auto hub = AruniTunnel::PairingInfo::decode( content.enrollmentCode );
		bool isGateway = false;
		const bool saved = GatewayState::update( [&hub, &isGateway]( GatewayState& state ) {
			if( state.enabled )
			{
				isGateway = true;
				return;
			}
			if( state.roamingEnabled && state.roamingHub.gatewayId == hub.gatewayId )
			{
				// already a roaming laptop of this office - keep the registration
				return;
			}
			state.roamingEnabled = true;
			state.roamingHub = hub;
			state.roamingRegistered = false;
		} );
		if( isGateway )
		{
			report.append( tr( "Roaming laptop skipped: this computer is the Aruni Gateway" ) );
		}
		else if( saved == false )
		{
			error = tr( "Could not save the roaming laptop settings (run as administrator)." );
			return false;
		}
		else
		{
			report.append( tr( "Roaming laptop of \"%1\"" ).arg( hub.siteName ) );
		}
	}

	return true;
}
