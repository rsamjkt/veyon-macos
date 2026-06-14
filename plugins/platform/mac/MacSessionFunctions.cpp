/*
 * MacSessionFunctions.cpp - implementation of MacSessionFunctions class
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

#include <QHostInfo>
#include <QProcessEnvironment>

#include <sys/sysctl.h>
#include <sys/time.h>
#include <time.h>

#include "MacSessionFunctions.h"


PlatformSessionFunctions::SessionId MacSessionFunctions::currentSessionId()
{
	// macOS has a single graphical console session per machine in the scope
	// currently supported by this port.
	return DefaultSessionId;
}



PlatformSessionFunctions::SessionUptime MacSessionFunctions::currentSessionUptime() const
{
	struct timeval bootTime{};
	size_t size = sizeof(bootTime);
	int mib[2] = { CTL_KERN, KERN_BOOTTIME };

	if( sysctl( mib, 2, &bootTime, &size, nullptr, 0 ) == 0 && bootTime.tv_sec != 0 )
	{
		const auto now = time( nullptr );
		return static_cast<SessionUptime>( now - bootTime.tv_sec );
	}

	return InvalidSessionUptime;
}



QString MacSessionFunctions::currentSessionClientAddress() const
{
	return {};
}



QString MacSessionFunctions::currentSessionClientName() const
{
	return {};
}



QString MacSessionFunctions::currentSessionHostName() const
{
	return QHostInfo::localHostName();
}



QString MacSessionFunctions::currentSessionType() const
{
	return QStringLiteral("console");
}



bool MacSessionFunctions::currentSessionHasUser() const
{
	return true;
}



PlatformSessionFunctions::EnvironmentVariables MacSessionFunctions::currentSessionEnvironmentVariables() const
{
	EnvironmentVariables variables;
	const auto env = QProcessEnvironment::systemEnvironment();
	const auto keys = env.keys();
	for( const auto& key : keys )
	{
		variables.insert( key, env.value( key ) );
	}
	return variables;
}



QVariant MacSessionFunctions::querySettingsValueInCurrentSession( const QString& key ) const
{
	Q_UNUSED(key)
	return {};
}
