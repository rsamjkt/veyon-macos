/*
 * InventoryStore.cpp - collected inventory of the computers
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

#include "GatewayState.h"
#include "InventoryStore.h"
#include "MonitoringCollector.h"


namespace {

QString csvField( QString value )
{
	value.replace( QLatin1Char('"'), QStringLiteral("\"\"") );
	if( value.contains( QLatin1Char(';') ) || value.contains( QLatin1Char('"') ) || value.contains( QLatin1Char('\n') ) )
	{
		value = QLatin1Char('"') + value + QLatin1Char('"');
	}
	return value;
}



bool writeCsv( const QString& fileName, const QList<QStringList>& rows )
{
	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return false;
	}
	// UTF-8 with BOM and ';' - what Excel with Indonesian settings opens by double-click
	file.write( "\xEF\xBB\xBF" );
	for( const auto& row : rows )
	{
		QStringList fields;
		for( const auto& field : row )
		{
			fields.append( csvField( field ) );
		}
		file.write( fields.join( QLatin1Char(';') ).toUtf8() + "\r\n" );
	}
	return true;
}



QString number( double value )
{
	return QLocale().toString( value, 'f', 1 );
}

}



QString InventoryStore::Record::fileName() const
{
	auto safeKey = key;
	safeKey.replace( QLatin1Char(':'), QLatin1Char('_') );
	return QDir( directory() ).filePath( MonitoringCollector::folderName( safeKey ) + QStringLiteral(".json") );
}



int InventoryStore::Record::lowestFreePercent() const
{
	int lowest = -1;
	const auto disks = data[QStringLiteral("disks")].toArray();
	for( const auto& value : disks )
	{
		const auto disk = value.toObject();
		const auto total = disk[QStringLiteral("totalGB")].toDouble();
		if( total > 0 )
		{
			const int percent = int( disk[QStringLiteral("freeGB")].toDouble() * 100 / total );
			lowest = lowest < 0 ? percent : qMin( lowest, percent );
		}
	}
	return lowest;
}



QString InventoryStore::directory()
{
	return QDir( GatewayState::directory() ).filePath( QStringLiteral("inventory") );
}



QList<InventoryStore::Record> InventoryStore::list()
{
	QList<Record> records;
	const QDir dir( directory() );
	const auto files = dir.entryList( { QStringLiteral("*.json") }, QDir::Files );
	for( const auto& file : files )
	{
		QFile f( dir.filePath( file ) );
		if( f.open( QFile::ReadOnly ) == false )
		{
			continue;
		}
		const auto json = QJsonDocument::fromJson( f.readAll() ).object();
		Record record;
		record.key = json[QStringLiteral("key")].toString();
		record.name = json[QStringLiteral("name")].toString();
		record.host = json[QStringLiteral("host")].toString();
		record.roaming = json[QStringLiteral("roaming")].toBool();
		record.lastSeen = QDateTime::fromString( json[QStringLiteral("lastSeen")].toString(), Qt::ISODate );
		record.collected = QDateTime::fromString( json[QStringLiteral("collected")].toString(), Qt::ISODate );
		record.data = json[QStringLiteral("data")].toObject();
		record.offlineAlerted = json[QStringLiteral("offlineAlerted")].toBool();
		record.diskAlertDate = json[QStringLiteral("diskAlertDate")].toString();
		if( record.key.isEmpty() == false )
		{
			records.append( record );
		}
	}

	std::sort( records.begin(), records.end(), []( const Record& a, const Record& b ) {
		return a.name.compare( b.name, Qt::CaseInsensitive ) < 0;
	} );
	return records;
}



InventoryStore::Record InventoryStore::load( const QString& key )
{
	const auto records = list();
	for( const auto& record : records )
	{
		if( record.key == key )
		{
			return record;
		}
	}
	Record record;
	record.key = key;
	return record;
}



bool InventoryStore::save( const Record& record )
{
	QDir().mkpath( directory() );
	QFile file( record.fileName() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return false;
	}
	file.write( QJsonDocument( QJsonObject{
		{ QStringLiteral("key"), record.key },
		{ QStringLiteral("name"), record.name },
		{ QStringLiteral("host"), record.host },
		{ QStringLiteral("roaming"), record.roaming },
		{ QStringLiteral("lastSeen"), record.lastSeen.toString( Qt::ISODate ) },
		{ QStringLiteral("collected"), record.collected.toString( Qt::ISODate ) },
		{ QStringLiteral("data"), record.data },
		{ QStringLiteral("offlineAlerted"), record.offlineAlerted },
		{ QStringLiteral("diskAlertDate"), record.diskAlertDate },
	} ).toJson( QJsonDocument::Compact ) );
	return true;
}



bool InventoryStore::exportComputersCsv( const QString& fileName )
{
	QList<QStringList> rows{ { tr( "Computer" ), tr( "Address" ), tr( "Manufacturer" ), tr( "Model" ), tr( "Serial number" ),
							   tr( "Operating system" ), tr( "Processor" ), tr( "Cores" ), tr( "RAM (GB)" ),
							   tr( "Disks (free/total GB)" ), tr( "MAC address" ), tr( "User" ), tr( "AruniControl" ),
							   tr( "Programs" ), tr( "Last seen" ), tr( "Collected" ) } };
	const auto records = list();
	for( const auto& record : records )
	{
		const auto& d = record.data;
		QStringList disks;
		for( const auto& value : d[QStringLiteral("disks")].toArray() )
		{
			const auto disk = value.toObject();
			disks.append( QStringLiteral("%1 %2/%3").arg( disk[QStringLiteral("path")].toString(),
														   number( disk[QStringLiteral("freeGB")].toDouble() ),
														   number( disk[QStringLiteral("totalGB")].toDouble() ) ) );
		}
		QStringList macs;
		for( const auto& value : d[QStringLiteral("network")].toArray() )
		{
			macs.append( value.toObject()[QStringLiteral("mac")].toString() );
		}
		rows.append( { record.name, record.host, d[QStringLiteral("manufacturer")].toString(), d[QStringLiteral("model")].toString(),
					   d[QStringLiteral("serial")].toString(), d[QStringLiteral("os")].toString(), d[QStringLiteral("cpu")].toString(),
					   QString::number( d[QStringLiteral("cores")].toInt() ),
					   number( d[QStringLiteral("ramMB")].toDouble() / 1024 ), disks.join( QStringLiteral(", ") ),
					   macs.join( QStringLiteral(", ") ), d[QStringLiteral("user")].toString(), d[QStringLiteral("version")].toString(),
					   QString::number( d[QStringLiteral("software")].toArray().size() ),
					   record.lastSeen.toLocalTime().toString( QStringLiteral("yyyy-MM-dd HH:mm") ),
					   record.collected.toLocalTime().toString( QStringLiteral("yyyy-MM-dd HH:mm") ) } );
	}
	return writeCsv( fileName, rows );
}



bool InventoryStore::exportSoftwareCsv( const QString& fileName )
{
	QList<QStringList> rows{ { tr( "Computer" ), tr( "Program" ), tr( "Version" ), tr( "Publisher" ), tr( "Installed" ) } };
	const auto records = list();
	for( const auto& record : records )
	{
		for( const auto& value : record.data[QStringLiteral("software")].toArray() )
		{
			const auto program = value.toObject();
			rows.append( { record.name, program[QStringLiteral("name")].toString(), program[QStringLiteral("version")].toString(),
						   program[QStringLiteral("publisher")].toString(), program[QStringLiteral("installed")].toString() } );
		}
	}
	return writeCsv( fileName, rows );
}
