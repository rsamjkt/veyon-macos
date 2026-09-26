/*
 * ComputerListFile.cpp - import/export of rooms and computers as a simple table
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

#include <QHash>
#include <QUuid>
#include <QRegularExpression>

#include "ComputerListFile.h"
#include "NetworkObject.h"


namespace {

enum Column { Location, Name, Host, Mac, ColumnCount };

// headings recognised (lower case, without spaces/punctuation)
int columnForHeading( const QString& heading )
{
	static const QRegularExpression strip{ QStringLiteral("[^a-z0-9]") };
	const auto key = heading.toLower().remove( strip );

	static const QHash<QString, int> headings{
		{ QStringLiteral("ruangan"), Location }, { QStringLiteral("ruang"), Location }, { QStringLiteral("lokasi"), Location },
		{ QStringLiteral("kelas"), Location }, { QStringLiteral("lab"), Location }, { QStringLiteral("location"), Location },
		{ QStringLiteral("room"), Location }, { QStringLiteral("group"), Location }, { QStringLiteral("grup"), Location },
		{ QStringLiteral("nama"), Name }, { QStringLiteral("namakomputer"), Name }, { QStringLiteral("komputer"), Name },
		{ QStringLiteral("name"), Name }, { QStringLiteral("computer"), Name }, { QStringLiteral("computername"), Name },
		{ QStringLiteral("pc"), Name },
		{ QStringLiteral("alamatip"), Host }, { QStringLiteral("alamat"), Host }, { QStringLiteral("ip"), Host },
		{ QStringLiteral("ipaddress"), Host }, { QStringLiteral("host"), Host }, { QStringLiteral("hostname"), Host },
		{ QStringLiteral("hostaddress"), Host }, { QStringLiteral("address"), Host }, { QStringLiteral("alamatiphostname"), Host },
		{ QStringLiteral("iphostname"), Host },
		{ QStringLiteral("mac"), Mac }, { QStringLiteral("macaddress"), Mac }, { QStringLiteral("alamatmac"), Mac },
	};

	return headings.value( key, -1 );
}

}



QStringList ComputerListFile::splitLine( const QString& line, QChar separator )
{
	// RFC 4180 style quoting as written by spreadsheets
	QStringList fields;
	QString field;
	bool quoted = false;
	for( int i = 0; i < line.size(); ++i )
	{
		const auto c = line.at( i );
		if( quoted )
		{
			if( c == QLatin1Char('"') )
			{
				if( i + 1 < line.size() && line.at( i + 1 ) == QLatin1Char('"') )
				{
					field += c;
					++i;
				}
				else
				{
					quoted = false;
				}
			}
			else
			{
				field += c;
			}
		}
		else if( c == QLatin1Char('"') && field.trimmed().isEmpty() )
		{
			quoted = true;
			field.clear();
		}
		else if( c == separator )
		{
			fields.append( field.trimmed() );
			field.clear();
		}
		else
		{
			field += c;
		}
	}
	fields.append( field.trimmed() );
	return fields;
}



QString ComputerListFile::normalizedMac( const QString& mac )
{
	static const QRegularExpression nonHex{ QStringLiteral("[^0-9A-Fa-f]") };
	auto hex = mac;
	hex.remove( nonHex );
	if( hex.size() != 12 )
	{
		return {};
	}
	hex = hex.toUpper();
	QStringList parts;
	for( int i = 0; i < 12; i += 2 )
	{
		parts.append( hex.mid( i, 2 ) );
	}
	return parts.join( QLatin1Char(':') );
}



ComputerListFile::ParseResult ComputerListFile::parse( const QString& text, const QString& defaultLocation )
{
	ParseResult result;

	auto content = text;
	if( content.startsWith( QChar( 0xFEFF ) ) )
	{
		content.remove( 0, 1 );
	}
	const auto lines = content.split( QRegularExpression( QStringLiteral("\\r\\n|\\n|\\r") ) );

	// the separator that splits the first line into the most fields
	QChar separator = QLatin1Char(';');
	for( const auto& line : lines )
	{
		if( line.trimmed().isEmpty() )
		{
			continue;
		}
		int best = 0;
		for( const auto candidate : { QLatin1Char('\t'), QLatin1Char(';'), QLatin1Char(',') } )
		{
			const auto count = int( splitLine( line, candidate ).size() );
			if( count > best )
			{
				best = count;
				separator = candidate;
			}
		}
		break;
	}

	int columns[ColumnCount] = { 0, 1, 2, 3 };
	bool headerSeen = false;

	for( int lineNumber = 0; lineNumber < lines.size(); ++lineNumber )
	{
		const auto& line = lines.at( lineNumber );
		if( line.trimmed().isEmpty() || line.trimmed().startsWith( QLatin1Char('#') ) )
		{
			continue;
		}

		const auto fields = splitLine( line, separator );

		// the first non-empty line may be the headings
		if( headerSeen == false )
		{
			headerSeen = true;
			int mapped[ColumnCount] = { -1, -1, -1, -1 };
			int recognised = 0;
			for( int i = 0; i < fields.size(); ++i )
			{
				const auto column = columnForHeading( fields.at( i ) );
				if( column >= 0 && mapped[column] < 0 )
				{
					mapped[column] = i;
					++recognised;
				}
			}
			if( recognised >= 2 )
			{
				for( int c = 0; c < ColumnCount; ++c )
				{
					columns[c] = mapped[c];
				}
				continue;
			}
			// no headings: a list of only names/addresses (one column) is
			// taken as addresses
			if( fields.size() == 1 )
			{
				columns[Location] = -1;
				columns[Name] = -1;
				columns[Host] = 0;
				columns[Mac] = -1;
			}
		}

		const auto value = [&fields]( int index ) {
			return index >= 0 && index < fields.size() ? fields.at( index ) : QString{};
		};

		Row row;
		row.location = value( columns[Location] );
		row.name = value( columns[Name] );
		row.host = value( columns[Host] );
		const auto mac = value( columns[Mac] );

		if( row.location.isEmpty() )
		{
			row.location = defaultLocation;
		}
		if( row.host.isEmpty() )
		{
			row.host = row.name;
		}
		if( row.name.isEmpty() )
		{
			row.name = row.host;
		}

		const auto rowLabel = tr( "Row %1" ).arg( lineNumber + 1 );
		if( row.host.isEmpty() )
		{
			result.problems.append( tr( "%1: no name or IP address - skipped" ).arg( rowLabel ) );
			continue;
		}
		if( row.host.contains( QLatin1Char(' ') ) )
		{
			result.problems.append( tr( "%1: \"%2\" is not a valid address - skipped" ).arg( rowLabel, row.host ) );
			continue;
		}
		if( row.location.isEmpty() )
		{
			result.problems.append( tr( "%1: no room given - skipped" ).arg( rowLabel ) );
			continue;
		}
		if( mac.isEmpty() == false )
		{
			row.mac = normalizedMac( mac );
			if( row.mac.isEmpty() )
			{
				result.problems.append( tr( "%1: MAC address \"%2\" ignored (invalid)" ).arg( rowLabel, mac ) );
			}
		}

		result.rows.append( row );
	}

	return result;
}



ComputerListFile::MergeResult ComputerListFile::merge( QJsonArray& networkObjects, const QList<Row>& rows, bool replace )
{
	MergeResult result;

	if( replace )
	{
		networkObjects = QJsonArray();
	}

	// room name (case-insensitive) -> uid
	QHash<QString, NetworkObject::Uid> locations;
	for( const auto& value : std::as_const( networkObjects ) )
	{
		const NetworkObject object( value.toObject() );
		if( object.type() == NetworkObject::Type::Location )
		{
			locations.insert( object.name().toLower(), object.uid() );
		}
	}

	for( const auto& row : rows )
	{
		auto locationUid = locations.value( row.location.toLower() );
		if( locationUid.isNull() )
		{
			const NetworkObject location( NetworkObject::Type::Location, row.location, {}, {}, {}, QUuid::createUuid() );
			locationUid = location.uid();
			locations.insert( row.location.toLower(), locationUid );
			networkObjects.append( location.toJson() );
			++result.locationsAdded;
		}

		bool updated = false;
		for( int i = 0; i < networkObjects.size(); ++i )
		{
			const NetworkObject existing( networkObjects.at( i ).toObject() );
			if( existing.type() == NetworkObject::Type::Host && existing.parentUid() == locationUid &&
				( existing.name().compare( row.name, Qt::CaseInsensitive ) == 0 ||
				  existing.hostAddress().compare( row.host, Qt::CaseInsensitive ) == 0 ) )
			{
				const NetworkObject replacement( NetworkObject::Type::Host, row.name, row.host,
												 row.mac.isEmpty() ? existing.macAddress() : row.mac,
												 {}, existing.uid(), locationUid );
				networkObjects[i] = replacement.toJson();
				++result.computersUpdated;
				updated = true;
				break;
			}
		}

		if( updated == false )
		{
			networkObjects.append( NetworkObject( NetworkObject::Type::Host, row.name, row.host, row.mac,
												  {}, NetworkObject::Uid::createUuid(), locationUid ).toJson() );
			++result.computersAdded;
		}
	}

	return result;
}



QByteArray ComputerListFile::toCsv( const QJsonArray& networkObjects )
{
	const auto field = []( QString value ) {
		if( value.contains( QLatin1Char(';') ) || value.contains( QLatin1Char('"') ) )
		{
			value.replace( QLatin1Char('"'), QStringLiteral("\"\"") );
			return QStringLiteral("\"%1\"").arg( value );
		}
		return value;
	};

	QHash<NetworkObject::Uid, QString> locationNames;
	for( const auto& value : networkObjects )
	{
		const NetworkObject object( value.toObject() );
		if( object.type() == NetworkObject::Type::Location )
		{
			locationNames.insert( object.uid(), object.name() );
		}
	}

	QString csv = QChar( 0xFEFF ) + tr( "Room" ) + QLatin1Char(';') + tr( "Name" ) + QLatin1Char(';') +
				  tr( "IP address" ) + QLatin1Char(';') + QStringLiteral("MAC\r\n");
	for( const auto& value : networkObjects )
	{
		const NetworkObject object( value.toObject() );
		if( object.type() == NetworkObject::Type::Host )
		{
			csv += field( locationNames.value( object.parentUid() ) ) + QLatin1Char(';') + field( object.name() ) +
				   QLatin1Char(';') + field( object.hostAddress() ) + QLatin1Char(';') + field( object.macAddress() ) +
				   QStringLiteral("\r\n");
		}
	}
	return csv.toUtf8();
}



QByteArray ComputerListFile::templateCsv()
{
	return ( QString( QChar( 0xFEFF ) ) + tr( "Room" ) + QLatin1Char(';') + tr( "Name" ) + QLatin1Char(';') +
			 tr( "IP address" ) + QStringLiteral(";MAC\r\n") +
			 QStringLiteral("Lab Komputer 1;PC-01;192.168.1.101;00:1A:2B:3C:4D:01\r\n"
							"Lab Komputer 1;PC-02;192.168.1.102;\r\n"
							"Ruang Guru;LAPTOP-GURU;192.168.1.50;\r\n") ).toUtf8();
}
