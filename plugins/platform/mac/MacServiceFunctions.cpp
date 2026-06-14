/*
 * MacServiceFunctions.cpp - implementation of MacServiceFunctions class
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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include <unistd.h>

#include "MacServiceFunctions.h"


// The macOS "service" is implemented as a per-user LaunchAgent that runs and
// supervises a veyon-server instance in the logged-in GUI session (which is
// where screen capture and input injection have to happen). Using a LaunchAgent
// keeps everything in user space - no administrator privileges required.

namespace {

QString launchAgentDir()
{
	return QDir::homePath() + QStringLiteral("/Library/LaunchAgents");
}


QString launchAgentPath( const QString& name )
{
	return launchAgentDir() + QLatin1Char('/') + name + QStringLiteral(".plist");
}


QString guiDomainTarget( const QString& name = {} )
{
	const auto domain = QStringLiteral("gui/%1").arg( getuid() );
	return name.isEmpty() ? domain : ( domain + QLatin1Char('/') + name );
}


int runLaunchctl( const QStringList& arguments )
{
	return QProcess::execute( QStringLiteral("/bin/launchctl"), arguments );
}


// Locate the veyon-server binary relative to the (veyon-service) path Veyon
// passes us. Works both for an installed/bundle layout (all binaries in one
// directory) and for the development build tree (build/server/veyon-server).
QString resolveServerBinary( const QString& serviceFilePath )
{
	const QFileInfo info( serviceFilePath );
	const auto dir = info.absolutePath();

	const QStringList candidates = {
		dir + QStringLiteral("/veyon-server"),
		dir + QStringLiteral("/../server/veyon-server"),
		dir + QStringLiteral("/../MacOS/veyon-server"),
	};

	for( const auto& candidate : candidates )
	{
		const QFileInfo c( candidate );
		if( c.exists() && c.isExecutable() )
		{
			return c.absoluteFilePath();
		}
	}

	return dir + QStringLiteral("/veyon-server");
}


bool writeLaunchAgentPlist( const QString& name, const QString& serverBinary,
							PlatformServiceFunctions::StartMode startMode )
{
	QDir().mkpath( launchAgentDir() );

	const bool runAtLoad = ( startMode == PlatformServiceFunctions::StartMode::Auto );

	const auto plist = QStringLiteral(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
		"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n"
		"<dict>\n"
		"\t<key>Label</key><string>%1</string>\n"
		"\t<key>ProgramArguments</key>\n"
		"\t<array>\n"
		"\t\t<string>%2</string>\n"
		"\t</array>\n"
		"\t<key>RunAtLoad</key><%3/>\n"
		"\t<key>KeepAlive</key><true/>\n"
		"\t<key>ProcessType</key><string>Interactive</string>\n"
		"</dict>\n"
		"</plist>\n" )
		.arg( name, serverBinary, runAtLoad ? QStringLiteral("true") : QStringLiteral("false") );

	QFile file( launchAgentPath( name ) );
	if( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) == false )
	{
		vCritical() << "MacServiceFunctions: cannot write LaunchAgent" << file.fileName();
		return false;
	}
	file.write( plist.toUtf8() );
	file.close();
	return true;
}

} // namespace



QString MacServiceFunctions::veyonServiceName() const
{
	return QStringLiteral("io.veyon.server");
}



bool MacServiceFunctions::isRegistered( const QString& name )
{
	return QFile::exists( launchAgentPath( name ) );
}



bool MacServiceFunctions::isRunning( const QString& name )
{
	// launchctl print returns 0 when the service is bootstrapped into the domain
	return runLaunchctl( { QStringLiteral("print"), guiDomainTarget( name ) } ) == 0;
}



bool MacServiceFunctions::start( const QString& name )
{
	if( isRegistered( name ) == false )
	{
		vCritical() << "MacServiceFunctions: service" << name << "is not registered yet";
		return false;
	}

	// bootstrap the agent into the GUI domain (ignore failure if already loaded)
	runLaunchctl( { QStringLiteral("bootstrap"), guiDomainTarget(), launchAgentPath( name ) } );

	// (re)start the service immediately
	return runLaunchctl( { QStringLiteral("kickstart"), QStringLiteral("-k"),
						   guiDomainTarget( name ) } ) == 0;
}



bool MacServiceFunctions::stop( const QString& name )
{
	return runLaunchctl( { QStringLiteral("bootout"), guiDomainTarget( name ) } ) == 0;
}



bool MacServiceFunctions::install( const QString& name, const QString& serviceFilePath,
								   StartMode startMode, const QString& displayName )
{
	Q_UNUSED(displayName)

	const auto serverBinary = resolveServerBinary( serviceFilePath );
	if( QFileInfo::exists( serverBinary ) == false )
	{
		vCritical() << "MacServiceFunctions: veyon-server binary not found near" << serviceFilePath;
		return false;
	}

	if( writeLaunchAgentPlist( name, serverBinary, startMode ) == false )
	{
		return false;
	}

	// load it into the GUI domain
	runLaunchctl( { QStringLiteral("bootout"), guiDomainTarget( name ) } ); // remove stale instance
	return runLaunchctl( { QStringLiteral("bootstrap"), guiDomainTarget(), launchAgentPath( name ) } ) == 0;
}



bool MacServiceFunctions::uninstall( const QString& name )
{
	runLaunchctl( { QStringLiteral("bootout"), guiDomainTarget( name ) } );
	return QFile::remove( launchAgentPath( name ) );
}



bool MacServiceFunctions::setStartMode( const QString& name, StartMode startMode )
{
	if( isRegistered( name ) == false )
	{
		return false;
	}

	// rewrite the plist keeping the existing program arguments
	QString serverBinary;
	QFile file( launchAgentPath( name ) );
	if( file.open( QIODevice::ReadOnly ) )
	{
		const auto content = QString::fromUtf8( file.readAll() );
		file.close();
		const int begin = content.indexOf( QStringLiteral("<string>"), content.indexOf( QStringLiteral("ProgramArguments") ) );
		if( begin >= 0 )
		{
			const int s = begin + int( qstrlen("<string>") );
			const int e = content.indexOf( QStringLiteral("</string>"), s );
			if( e > s )
			{
				serverBinary = content.mid( s, e - s );
			}
		}
	}

	if( serverBinary.isEmpty() )
	{
		return false;
	}

	return writeLaunchAgentPlist( name, serverBinary, startMode );
}



bool MacServiceFunctions::runAsService( const QString& name, const ServiceEntryPoint& serviceEntryPoint )
{
	Q_UNUSED(name)

	// When launched by launchd the process simply runs the regular service
	// entry point in the foreground; launchd handles lifetime management.
	if( serviceEntryPoint )
	{
		serviceEntryPoint();
	}

	return true;
}



void MacServiceFunctions::manageServerInstances()
{
	// Not used on macOS: the LaunchAgent runs and supervises veyon-server
	// directly, so there is no separate service supervisor process.
}
