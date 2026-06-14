/*
 * MacUserFunctions.cpp - implementation of MacUserFunctions class
 *
 * Copyright (c) 2026 Veyon Community / macOS port
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

#include <QList>
#include <QString>
#include <QStringList>

#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>

#include "MacUserFunctions.h"


QString MacUserFunctions::queryCurrentUserProperty( UserProperty property )
{
	const auto pw = getpwuid( getuid() );
	if( pw == nullptr )
	{
		return {};
	}

	switch( property )
	{
	case UserProperty::LoginName:
		return QString::fromUtf8( pw->pw_name );
	case UserProperty::FullName:
	{
		// pw_gecos may contain comma-separated fields; the first is the full name
		const auto gecos = QString::fromUtf8( pw->pw_gecos );
		return gecos.section( QLatin1Char(','), 0, 0 );
	}
	case UserProperty::None:
		break;
	}

	return {};
}



QStringList MacUserFunctions::userGroups( bool queryDomainGroups )
{
	Q_UNUSED(queryDomainGroups)

	QStringList groups;
	setgrent();
	while( const auto grp = getgrent() )
	{
		const auto name = QString::fromUtf8( grp->gr_name );
		if( name.startsWith( QLatin1Char('_') ) == false )
		{
			groups.append( name );
		}
	}
	endgrent();

	groups.removeDuplicates();
	groups.sort();
	return groups;
}



QStringList MacUserFunctions::groupsOfUser( const QString& username, bool queryDomainGroups )
{
	Q_UNUSED(queryDomainGroups)

	QStringList groups;

	const auto pw = getpwnam( username.toUtf8().constData() );
	if( pw == nullptr )
	{
		return groups;
	}

	// macOS getgrouplist() does not reliably report the required size when the
	// supplied buffer is too small, so grow the buffer until the call succeeds.
	QList<int> gids;
	int capacity = NGROUPS_MAX > 0 ? NGROUPS_MAX : 64;
	const int maxCapacity = 65536;
	int ngroups = 0;

	while( true )
	{
		gids.resize( capacity );
		ngroups = capacity;
		if( getgrouplist( pw->pw_name, static_cast<int>( pw->pw_gid ), gids.data(), &ngroups ) >= 0 )
		{
			break;
		}
		if( capacity >= maxCapacity )
		{
			// give up enlarging; use whatever fit into the buffer
			break;
		}
		capacity = qMin( capacity * 2, maxCapacity );
	}

	for( int i = 0; i < ngroups && i < gids.size(); ++i )
	{
		if( const auto grp = getgrgid( static_cast<gid_t>( gids.at(i) ) ) )
		{
			groups.append( QString::fromUtf8( grp->gr_name ) );
		}
	}

	groups.removeDuplicates();
	groups.sort();
	return groups;
}



QString MacUserFunctions::userGroupSecurityIdentifier( const QString& groupName )
{
	// macOS does not use Windows-style SIDs; return the numeric GID as identifier.
	const auto grp = getgrnam( groupName.toUtf8().constData() );
	if( grp )
	{
		return QString::number( grp->gr_gid );
	}
	return {};
}



bool MacUserFunctions::isAnyUserLoggedOn()
{
	return true;
}



bool MacUserFunctions::prepareLogon( const QString& username, const Password& password )
{
	// Automated logon is not supported on macOS by this port.
	Q_UNUSED(username)
	Q_UNUSED(password)
	return false;
}



bool MacUserFunctions::performLogon( const QString& username, const Password& password )
{
	Q_UNUSED(username)
	Q_UNUSED(password)
	return false;
}



void MacUserFunctions::logoff()
{
	// TODO: trigger logout via Apple Events ('logout' to loginwindow).
}



bool MacUserFunctions::authenticate( const QString& username, const Password& password )
{
	// TODO: authenticate against Open Directory / PAM ("authorization" service).
	Q_UNUSED(username)
	Q_UNUSED(password)
	return false;
}
