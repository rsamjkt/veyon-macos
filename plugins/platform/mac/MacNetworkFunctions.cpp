/*
 * MacNetworkFunctions.cpp - implementation of MacNetworkFunctions class
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

#include <QProcess>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "MacNetworkFunctions.h"


PlatformNetworkFunctions::PingResult MacNetworkFunctions::ping( const QString& hostAddress )
{
	QProcess pingProcess;
	const int timeoutSeconds = qMax( 1, PingTimeout / 1000 );
	pingProcess.start( QStringLiteral("/sbin/ping"),
					   { QStringLiteral("-c1"),
						 QStringLiteral("-t%1").arg( timeoutSeconds ),
						 hostAddress } );

	if( pingProcess.waitForFinished( PingProcessTimeout ) == false )
	{
		pingProcess.kill();
		return PingResult::TimedOut;
	}

	if( pingProcess.exitStatus() == QProcess::NormalExit && pingProcess.exitCode() == 0 )
	{
		return PingResult::ReplyReceived;
	}

	const QString output = QString::fromUtf8( pingProcess.readAllStandardError() ) +
						   QString::fromUtf8( pingProcess.readAllStandardOutput() );

	if( output.contains( QStringLiteral("cannot resolve"), Qt::CaseInsensitive ) ||
		output.contains( QStringLiteral("Unknown host"), Qt::CaseInsensitive ) ||
		output.contains( QStringLiteral("name or service not known"), Qt::CaseInsensitive ) )
	{
		return PingResult::NameResolutionFailed;
	}

	return PingResult::TimedOut;
}



bool MacNetworkFunctions::configureFirewallException( const QString& applicationPath, const QString& description, bool enabled )
{
	// The macOS application firewall is managed via socketfilterfw and requires
	// administrator privileges. Not configured automatically for now.
	Q_UNUSED(applicationPath)
	Q_UNUSED(description)
	Q_UNUSED(enabled)
	return true;
}



bool MacNetworkFunctions::configureSocketKeepalive( Socket socket, bool enabled, int idleTime, int interval, int probes )
{
	const int fd = static_cast<int>( socket );
	const int enableValue = enabled ? 1 : 0;

	bool ok = setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, &enableValue, sizeof(enableValue) ) == 0;

	if( enabled )
	{
#ifdef TCP_KEEPALIVE
		ok &= setsockopt( fd, IPPROTO_TCP, TCP_KEEPALIVE, &idleTime, sizeof(idleTime) ) == 0;
#endif
#ifdef TCP_KEEPINTVL
		ok &= setsockopt( fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval) ) == 0;
#endif
#ifdef TCP_KEEPCNT
		ok &= setsockopt( fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes) ) == 0;
#endif
	}

	return ok;
}



QNetworkInterface MacNetworkFunctions::defaultRouteNetworkInterface()
{
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		const auto flags = iface.flags();
		if( flags.testFlag( QNetworkInterface::IsUp ) &&
			flags.testFlag( QNetworkInterface::IsRunning ) &&
			!flags.testFlag( QNetworkInterface::IsLoopBack ) &&
			!iface.addressEntries().isEmpty() )
		{
			return iface;
		}
	}

	return {};
}



int MacNetworkFunctions::networkInterfaceSpeedInMBitPerSecond( const QNetworkInterface& networkInterface )
{
	// Querying link speed on macOS requires IOKit traversal; report unknown for now.
	Q_UNUSED(networkInterface)
	return 0;
}
