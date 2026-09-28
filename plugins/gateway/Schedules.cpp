/*
 * Schedules.cpp - automatic actions at set times
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
#include <QLocale>
#include <QUuid>

#include "GatewayState.h"
#include "Schedules.h"


namespace {

QString schedulesFile( const QString& name )
{
	return QDir( GatewayState::directory() ).filePath( name );
}



bool writeJsonFile( const QString& fileName, const QJsonDocument& document )
{
	QDir().mkpath( QFileInfo( fileName ).absolutePath() );
	const auto tempPath = fileName + QStringLiteral(".tmp");
	QFile file( tempPath );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return false;
	}
	file.write( document.toJson() );
	file.close();
	QFile::remove( fileName );
	return QFile::rename( tempPath, fileName );
}

}



QString Schedules::path()
{
	return schedulesFile( QStringLiteral("schedules.json") );
}



Schedules Schedules::load()
{
	Schedules schedules;

	QFile file( path() );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return schedules;
	}

	const auto array = QJsonDocument::fromJson( file.readAll() ).object()[QStringLiteral("rules")].toArray();
	for( const auto& value : array )
	{
		const auto object = value.toObject();
		Rule rule;
		bool ok = false;
		rule.id = object[QStringLiteral("id")].toString();
		rule.name = object[QStringLiteral("name")].toString();
		rule.enabled = object[QStringLiteral("enabled")].toBool( true );
		rule.days = object[QStringLiteral("days")].toInt( 0x1f ) & 0x7f;
		rule.time = QTime::fromString( object[QStringLiteral("time")].toString(), QStringLiteral("HH:mm") );
		rule.action = actionFromKey( object[QStringLiteral("action")].toString(), &ok );
		rule.room = object[QStringLiteral("room")].toString();
		rule.sites = object[QStringLiteral("sites")].toVariant().toStringList();
		rule.text = object[QStringLiteral("text")].toString();
		if( ok && rule.time.isValid() && rule.id.isEmpty() == false )
		{
			schedules.rules.append( rule );
		}
	}

	return schedules;
}



bool Schedules::save() const
{
	QJsonArray array;
	for( const auto& rule : rules )
	{
		array.append( QJsonObject{
			{ QStringLiteral("id"), rule.id.isEmpty() ? QUuid::createUuid().toString( QUuid::WithoutBraces ) : rule.id },
			{ QStringLiteral("name"), rule.name },
			{ QStringLiteral("enabled"), rule.enabled },
			{ QStringLiteral("days"), rule.days },
			{ QStringLiteral("time"), rule.time.toString( QStringLiteral("HH:mm") ) },
			{ QStringLiteral("action"), actionKey( rule.action ) },
			{ QStringLiteral("room"), rule.room },
			{ QStringLiteral("sites"), QJsonArray::fromStringList( rule.sites ) },
			{ QStringLiteral("text"), rule.text },
		} );
	}

	return writeJsonFile( path(), QJsonDocument( QJsonObject{ { QStringLiteral("rules"), array } } ) );
}



QList<Schedules::Action> Schedules::actions()
{
	return { Action::PowerOn, Action::PowerDown, Action::Reboot, Action::LockScreen, Action::UnlockScreen,
			 Action::BlockInternet, Action::AllowInternet, Action::BlockSites, Action::UnblockSites,
			 Action::Message, Action::StartExam, Action::EndExam, Action::BlockUsb, Action::AllowUsb,
			 Action::BlockPrinting, Action::AllowPrinting };
}



QString Schedules::actionKey( Action action )
{
	switch( action )
	{
	case Action::PowerOn: return QStringLiteral("powerOn");
	case Action::PowerDown: return QStringLiteral("powerDown");
	case Action::Reboot: return QStringLiteral("reboot");
	case Action::LockScreen: return QStringLiteral("lock");
	case Action::UnlockScreen: return QStringLiteral("unlock");
	case Action::BlockInternet: return QStringLiteral("blockInternet");
	case Action::AllowInternet: return QStringLiteral("allowInternet");
	case Action::BlockSites: return QStringLiteral("blockSites");
	case Action::UnblockSites: return QStringLiteral("unblockSites");
	case Action::Message: return QStringLiteral("message");
	case Action::StartExam: return QStringLiteral("startExam");
	case Action::EndExam: return QStringLiteral("endExam");
	case Action::BlockUsb: return QStringLiteral("blockUsb");
	case Action::AllowUsb: return QStringLiteral("allowUsb");
	case Action::BlockPrinting: return QStringLiteral("blockPrinting");
	case Action::AllowPrinting: return QStringLiteral("allowPrinting");
	case Action::PushRoles: return QStringLiteral("pushRoles");
	}
	return {};
}



Schedules::Action Schedules::actionFromKey( const QString& key, bool* ok )
{
	for( const auto action : actions() )
	{
		if( actionKey( action ) == key )
		{
			if( ok )
			{
				*ok = true;
			}
			return action;
		}
	}
	if( ok )
	{
		*ok = false;
	}
	return Action::PowerOn;
}



QString Schedules::actionName( Action action )
{
	switch( action )
	{
	case Action::PowerOn: return tr( "Power on (Wake-on-LAN)" );
	case Action::PowerDown: return tr( "Power down" );
	case Action::Reboot: return tr( "Reboot" );
	case Action::LockScreen: return tr( "Lock screen" );
	case Action::UnlockScreen: return tr( "Unlock screen" );
	case Action::BlockInternet: return tr( "Block internet" );
	case Action::AllowInternet: return tr( "Allow internet" );
	case Action::BlockSites: return tr( "Block websites" );
	case Action::UnblockSites: return tr( "Unblock websites" );
	case Action::Message: return tr( "Send message" );
	case Action::StartExam: return tr( "Start exam mode" );
	case Action::EndExam: return tr( "End exam mode" );
	case Action::BlockUsb: return tr( "Block USB storage" );
	case Action::AllowUsb: return tr( "Allow USB storage" );
	case Action::BlockPrinting: return tr( "Block printing" );
	case Action::AllowPrinting: return tr( "Allow printing" );
	case Action::PushRoles: return tr( "Send admin roles" );
	}
	return {};
}



bool Schedules::needsSites( Action action )
{
	return action == Action::BlockSites || action == Action::StartExam;
}



QString Schedules::daysText( int days )
{
	days &= 0x7f;
	if( days == 0x7f )
	{
		return tr( "Every day" );
	}
	if( days == 0x1f )
	{
		return tr( "Monday-Friday" );
	}
	if( days == 0x3f )
	{
		return tr( "Monday-Saturday" );
	}

	QStringList names;
	for( int day = 1; day <= 7; ++day )
	{
		if( days & ( 1 << ( day - 1 ) ) )
		{
			names.append( QLocale().dayName( day, QLocale::ShortFormat ) );
		}
	}
	return names.isEmpty() ? tr( "Never" ) : names.join( QStringLiteral(", ") );
}



void Schedules::requestRun( const QString& ruleId )
{
	QFile file( schedulesFile( QStringLiteral("schedules-run") ) );
	if( file.open( QFile::Append ) )
	{
		file.write( ruleId.toUtf8() + '\n' );
	}
}



QStringList Schedules::takeRunRequests()
{
	const auto fileName = schedulesFile( QStringLiteral("schedules-run") );
	QFile file( fileName );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	const auto content = QString::fromUtf8( file.readAll() );
	file.close();
	file.remove();
	return content.split( QLatin1Char('\n'), Qt::SkipEmptyParts );
}



QJsonObject Schedules::readStatus()
{
	QFile file( schedulesFile( QStringLiteral("schedules-status.json") ) );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	return QJsonDocument::fromJson( file.readAll() ).object();
}



void Schedules::writeStatus( const QJsonObject& status )
{
	writeJsonFile( schedulesFile( QStringLiteral("schedules-status.json") ), QJsonDocument( status ) );
}
