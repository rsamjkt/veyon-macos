/*
 * MacCoreFunctions.cpp - implementation of MacCoreFunctions class
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

#include <QCoreApplication>
#include <QProcess>
#include <QScreen>
#include <QSocketNotifier>
#include <QWidget>

#include <libproc.h>
#include <syslog.h>
#include <unistd.h>

#include "MacCoreFunctions.h"


bool MacCoreFunctions::applyConfiguration()
{
	return true;
}



bool MacCoreFunctions::prepareSessionBusAccess()
{
	// No D-Bus session bus on macOS.
	return true;
}



void MacCoreFunctions::initNativeLoggingSystem( const QString& appName )
{
	m_loggingAppName = appName.toUtf8();
	openlog( m_loggingAppName.constData(), LOG_PID | LOG_NDELAY, LOG_USER );
}



void MacCoreFunctions::writeToNativeLoggingSystem( const QString& message, Logger::LogLevel loglevel )
{
	int priority = LOG_INFO;
	switch( loglevel )
	{
	case Logger::LogLevel::Critical: priority = LOG_CRIT; break;
	case Logger::LogLevel::Error: priority = LOG_ERR; break;
	case Logger::LogLevel::Warning: priority = LOG_WARNING; break;
	case Logger::LogLevel::Info: priority = LOG_INFO; break;
	case Logger::LogLevel::Debug: priority = LOG_DEBUG; break;
	default: return;
	}

	syslog( priority, "%s", message.toUtf8().constData() );
}



QObject* MacCoreFunctions::notifyOnStandardInputReadyRead( const NotifierCallback& callback )
{
	auto* notifier = new QSocketNotifier( STDIN_FILENO, QSocketNotifier::Read );
	QObject::connect( notifier, &QSocketNotifier::activated, QCoreApplication::instance(),
					  [notifier, callback]() { callback( notifier ); } );

	return notifier;
}



void MacCoreFunctions::reboot()
{
	QProcess::startDetached( QStringLiteral("/usr/bin/osascript"),
							 { QStringLiteral("-e"),
							   QStringLiteral("tell application \"System Events\" to restart") } );
}



void MacCoreFunctions::powerDown( bool installUpdates )
{
	Q_UNUSED(installUpdates)
	QProcess::startDetached( QStringLiteral("/usr/bin/osascript"),
							 { QStringLiteral("-e"),
							   QStringLiteral("tell application \"System Events\" to shut down") } );
}



void MacCoreFunctions::raiseWindow( QWidget* widget, bool stayOnTop )
{
	if( widget == nullptr )
	{
		return;
	}

	widget->activateWindow();
	widget->raise();

	if( stayOnTop )
	{
		widget->setWindowFlag( Qt::WindowStaysOnTopHint, true );
		widget->show();
	}
}



void MacCoreFunctions::disableScreenSaver()
{
	// TODO: prevent display sleep using IOPMAssertionCreateWithName
	// (kIOPMAssertionTypePreventUserIdleDisplaySleep).
}



void MacCoreFunctions::restoreScreenSaverSettings()
{
	// TODO: release the IOPMAssertion created in disableScreenSaver().
}



void MacCoreFunctions::setSystemUiState( bool enabled )
{
	// TODO: hide/show the menu bar and Dock (kiosk-style) via the
	// Presentation Options API. No-op for now.
	Q_UNUSED(enabled)
}



QString MacCoreFunctions::activeDesktopName()
{
	return {};
}



bool MacCoreFunctions::isRunningAsAdmin() const
{
	return geteuid() == 0;
}



bool MacCoreFunctions::runProgramAsAdmin( const QString& program, const QStringList& parameters )
{
	QStringList quotedArgs;
	quotedArgs.reserve( parameters.size() + 1 );
	quotedArgs.append( QStringLiteral("'%1'").arg( program ) );
	for( const auto& parameter : parameters )
	{
		quotedArgs.append( QStringLiteral("'%1'").arg( parameter ) );
	}

	const auto script = QStringLiteral("do shell script \"%1\" with administrator privileges")
							.arg( quotedArgs.join( QLatin1Char(' ') ) );

	return QProcess::execute( QStringLiteral("/usr/bin/osascript"),
							  { QStringLiteral("-e"), script } ) == 0;
}



bool MacCoreFunctions::runProgramAsUser( const QString& program, const QStringList& parameters,
										 const QString& username, const QString& desktop,
										 const QByteArray& stdInData )
{
	// The macOS server is a LaunchAgent and therefore already runs inside the
	// session of the very user we would have to switch to - no privilege
	// juggling required, unlike on Linux and Windows.
	Q_UNUSED(username)
	Q_UNUSED(desktop)

	// The process cannot be started detached: the worker authentication token is
	// handed over through its standard input, which requires a channel to it.
	auto* process = new QProcess;

	if( stdInData.isEmpty() == false )
	{
		QObject::connect( process, &QProcess::started, process, [process, stdInData]() {
			process->write( stdInData );
			process->closeWriteChannel();
		} );
	}

	// QProcess reports a failed start asynchronously, so log it here rather than
	// letting the caller wonder why the program never showed up
	QObject::connect( process, &QProcess::errorOccurred, process,
					  [program]( QProcess::ProcessError error ) {
		vWarning() << "failed to run" << program << "as user:" << error;
	} );

	QObject::connect( process, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ),
					  process, &QProcess::deleteLater );

	process->start( program, parameters );

	return true;
}



QString MacCoreFunctions::genericUrlHandler() const
{
	return QStringLiteral("/usr/bin/open");
}



QString MacCoreFunctions::queryDisplayDeviceName( const QScreen& screen ) const
{
	return screen.name();
}



QString MacCoreFunctions::getApplicationName( ProcessId processId ) const
{
	char pathBuffer[PROC_PIDPATHINFO_MAXSIZE]{};
	if( proc_pidpath( static_cast<int>( processId ), pathBuffer, sizeof(pathBuffer) ) > 0 )
	{
		return QString::fromUtf8( pathBuffer );
	}

	return {};
}
