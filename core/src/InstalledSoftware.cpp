/*
 * InstalledSoftware.cpp - the software installed on this computer
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
#include <QFileInfo>
#include <QJsonObject>
#include <QSettings>

#include "InstalledSoftware.h"


QList<InstalledSoftware::Entry> InstalledSoftware::list()
{
	QList<Entry> entries;

#if defined(Q_OS_WIN)
	const auto uninstallKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
	for( const auto format : { QSettings::Registry64Format, QSettings::Registry32Format } )
	{
		QSettings registry( uninstallKey, format );
		const auto keys = registry.childGroups();
		for( const auto& key : keys )
		{
			registry.beginGroup( key );
			Entry entry;
			entry.id = key;
			entry.name = registry.value( QStringLiteral("DisplayName") ).toString().trimmed();
			entry.version = registry.value( QStringLiteral("DisplayVersion") ).toString().trimmed();
			entry.publisher = registry.value( QStringLiteral("Publisher") ).toString().trimmed();
			const auto date = registry.value( QStringLiteral("InstallDate") ).toString();
			if( date.size() == 8 )
			{
				entry.installDate = date.left( 4 ) + QLatin1Char('-') + date.mid( 4, 2 ) + QLatin1Char('-') + date.right( 2 );
			}
			entry.uninstall = registry.value( QStringLiteral("UninstallString") ).toString();
			entry.quietUninstall = registry.value( QStringLiteral("QuietUninstallString") ).toString();
			const bool hidden = registry.value( QStringLiteral("SystemComponent") ).toInt() == 1 ||
								registry.contains( QStringLiteral("ParentKeyName") ) ||
								registry.value( QStringLiteral("ReleaseType") ).toString().contains( QStringLiteral("Update") );
			registry.endGroup();

			if( entry.name.isEmpty() || hidden )
			{
				continue;
			}
			// the same program listed in both views of the registry
			const bool duplicate = std::any_of( entries.cbegin(), entries.cend(), [&entry]( const Entry& other ) {
				return other.name == entry.name && other.version == entry.version;
			} );
			if( duplicate == false )
			{
				entries.append( entry );
			}
		}
	}
#elif defined(Q_OS_MACOS)
	for( const auto& folder : { QStringLiteral("/Applications"), QStringLiteral("/Applications/Utilities") } )
	{
		const auto apps = QDir( folder ).entryInfoList( { QStringLiteral("*.app") }, QDir::Dirs | QDir::NoDotAndDotDot );
		for( const auto& app : apps )
		{
			const QSettings info( app.filePath() + QStringLiteral("/Contents/Info.plist"), QSettings::NativeFormat );
			Entry entry;
			entry.id = app.filePath();
			entry.name = info.value( QStringLiteral("CFBundleDisplayName"),
									 info.value( QStringLiteral("CFBundleName"), app.completeBaseName() ) ).toString();
			entry.version = info.value( QStringLiteral("CFBundleShortVersionString"),
										info.value( QStringLiteral("CFBundleVersion") ) ).toString();
			entry.installDate = app.birthTime().date().toString( Qt::ISODate );
			entries.append( entry );
		}
	}
#endif

	std::sort( entries.begin(), entries.end(), []( const Entry& a, const Entry& b ) {
		return a.name.compare( b.name, Qt::CaseInsensitive ) < 0;
	} );
	return entries;
}



QJsonArray InstalledSoftware::toJson( const QList<Entry>& entries, bool withCommands )
{
	QJsonArray array;
	for( const auto& entry : entries )
	{
		QJsonObject object{
			{ QStringLiteral("name"), entry.name },
			{ QStringLiteral("version"), entry.version },
			{ QStringLiteral("publisher"), entry.publisher },
			{ QStringLiteral("installed"), entry.installDate },
		};
		if( withCommands )
		{
			object[QStringLiteral("id")] = entry.id;
			object[QStringLiteral("removable")] = entry.uninstall.isEmpty() == false || entry.quietUninstall.isEmpty() == false;
		}
		array.append( object );
	}
	return array;
}
