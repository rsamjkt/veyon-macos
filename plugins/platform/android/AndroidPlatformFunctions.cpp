/*
 * AndroidPlatformFunctions.cpp - platform function groups for Android
 *
 * Copyright (c) 2026 AruniControl Community / Android port
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
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHostInfo>
#include <QJniObject>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QWidget>

#include <android/log.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>

#include "AndroidPlatformFunctions.h"


static QString androidBuildField( const char* name )
{
	return QJniObject::getStaticObjectField<jstring>( "android/os/Build", name ).toString();
}



// ---------------------------------------------------------------------------
// Core

void AndroidCoreFunctions::initNativeLoggingSystem( const QString& appName )
{
	m_loggingTag = appName.toUtf8();
}



void AndroidCoreFunctions::writeToNativeLoggingSystem( const QString& message, Logger::LogLevel loglevel )
{
	int priority = ANDROID_LOG_INFO;
	switch( loglevel )
	{
	case Logger::LogLevel::Critical: priority = ANDROID_LOG_FATAL; break;
	case Logger::LogLevel::Error: priority = ANDROID_LOG_ERROR; break;
	case Logger::LogLevel::Warning: priority = ANDROID_LOG_WARN; break;
	case Logger::LogLevel::Info: priority = ANDROID_LOG_INFO; break;
	case Logger::LogLevel::Debug: priority = ANDROID_LOG_DEBUG; break;
	default: return;
	}

	__android_log_write( priority, m_loggingTag.constData(), message.toUtf8().constData() );
}



QObject* AndroidCoreFunctions::notifyOnStandardInputReadyRead( const NotifierCallback& callback )
{
	// there is no standard input for an Android app
	Q_UNUSED(callback)
	return nullptr;
}



void AndroidCoreFunctions::raiseWindow( QWidget* widget, bool stayOnTop )
{
	Q_UNUSED(stayOnTop)

	if( widget )
	{
		widget->activateWindow();
		widget->raise();
	}
}



bool AndroidCoreFunctions::runProgramAsAdmin( const QString& program, const QStringList& parameters )
{
	Q_UNUSED(program)
	Q_UNUSED(parameters)
	return false;
}



bool AndroidCoreFunctions::runProgramAsUser( const QString& program, const QStringList& parameters,
											 const QString& username, const QString& desktop,
											 const QByteArray& stdInData )
{
	Q_UNUSED(program)
	Q_UNUSED(parameters)
	Q_UNUSED(username)
	Q_UNUSED(desktop)
	Q_UNUSED(stdInData)
	return false;
}



QString AndroidCoreFunctions::queryDisplayDeviceName( const QScreen& screen ) const
{
	return screen.name();
}



QString AndroidCoreFunctions::getApplicationName( ProcessId processId ) const
{
	Q_UNUSED(processId)
	return {};
}



// ---------------------------------------------------------------------------
// Filesystem - everything lives in the app's private storage

// Android reports app directories below /data/user/0, which is a symlink to
// /data/data - Veyon refuses symlinked paths for logs and keys, so resolve them
static QString canonicalLocation( QStandardPaths::StandardLocation location )
{
	const auto path = QStandardPaths::writableLocation( location );
	QDir().mkpath( path );
	const auto canonical = QDir( path ).canonicalPath();
	return canonical.isEmpty() ? path : canonical;
}



QString AndroidFilesystemFunctions::personalAppDataPath() const
{
	return canonicalLocation( QStandardPaths::AppDataLocation );
}



QString AndroidFilesystemFunctions::globalAppDataPath() const
{
	return canonicalLocation( QStandardPaths::AppDataLocation );
}



QString AndroidFilesystemFunctions::globalTempPath() const
{
	return canonicalLocation( QStandardPaths::TempLocation );
}



QString AndroidFilesystemFunctions::fileOwnerGroup( const QString& filePath )
{
	return QFileInfo( filePath ).group();
}



bool AndroidFilesystemFunctions::setFileOwnerGroup( const QString& filePath, const QString& ownerGroup )
{
	// app-private storage has a single owner; nothing to do
	Q_UNUSED(filePath)
	Q_UNUSED(ownerGroup)
	return true;
}



bool AndroidFilesystemFunctions::setFileOwnerGroupPermissions( const QString& filePath, QFile::Permissions permissions )
{
	Q_UNUSED(filePath)
	Q_UNUSED(permissions)
	return true;
}



bool AndroidFilesystemFunctions::openFileSafely( QFile* file, QFile::OpenMode openMode, QFile::Permissions permissions )
{
	if( file == nullptr || file->open( openMode ) == false )
	{
		return false;
	}

	file->setPermissions( permissions );
	return true;
}



PlatformCoreFunctions::ProcessId AndroidFilesystemFunctions::findFileLockingProcess( const QString& filePath ) const
{
	Q_UNUSED(filePath)
	return PlatformCoreFunctions::InvalidProcessId;
}



// ---------------------------------------------------------------------------
// Network

PlatformNetworkFunctions::PingResult AndroidNetworkFunctions::ping( const QString& hostAddress )
{
	// /system/bin/ping is available to apps on stock Android (setuid-free ICMP
	// sockets); if a vendor removed it the result is simply "unknown" and the
	// Master falls back to the VNC connection state
	QProcess pingProcess;
	pingProcess.start( QStringLiteral("/system/bin/ping"),
					   { QStringLiteral("-c1"),
						 QStringLiteral("-W%1").arg( qMax( 1, PingTimeout / 1000 ) ),
						 hostAddress } );

	if( pingProcess.waitForStarted( PingProcessTimeout ) == false )
	{
		return PingResult::Unknown;
	}

	if( pingProcess.waitForFinished( PingProcessTimeout ) == false )
	{
		pingProcess.kill();
		return PingResult::TimedOut;
	}

	if( pingProcess.exitStatus() == QProcess::NormalExit && pingProcess.exitCode() == 0 )
	{
		return PingResult::ReplyReceived;
	}

	const auto output = QString::fromUtf8( pingProcess.readAllStandardError() + pingProcess.readAllStandardOutput() );
	if( output.contains( QStringLiteral("unknown host"), Qt::CaseInsensitive ) )
	{
		return PingResult::NameResolutionFailed;
	}

	return PingResult::TimedOut;
}



bool AndroidNetworkFunctions::configureFirewallException( const QString& applicationPath, const QString& description, bool enabled )
{
	Q_UNUSED(applicationPath)
	Q_UNUSED(description)
	Q_UNUSED(enabled)
	return true;
}



bool AndroidNetworkFunctions::configureSocketKeepalive( Socket socket, bool enabled, int idleTime, int interval, int probes )
{
	const int fd = static_cast<int>( socket );
	const int enableValue = enabled ? 1 : 0;

	bool ok = setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, &enableValue, sizeof(enableValue) ) == 0;

	if( enabled )
	{
		// Veyon passes milliseconds, the socket options take seconds
		const int idleSeconds = qMax( 1, idleTime / 1000 );
		const int intervalSeconds = qMax( 1, interval / 1000 );
		ok &= setsockopt( fd, IPPROTO_TCP, TCP_KEEPIDLE, &idleSeconds, sizeof(idleSeconds) ) == 0;
		ok &= setsockopt( fd, IPPROTO_TCP, TCP_KEEPINTVL, &intervalSeconds, sizeof(intervalSeconds) ) == 0;
		ok &= setsockopt( fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes) ) == 0;
	}

	return ok;
}



QNetworkInterface AndroidNetworkFunctions::defaultRouteNetworkInterface()
{
	const auto interfaces = QNetworkInterface::allInterfaces();

	// prefer Wi-Fi/Ethernet over mobile data
	for( const auto& type : { QNetworkInterface::Wifi, QNetworkInterface::Ethernet, QNetworkInterface::Unknown } )
	{
		for( const auto& iface : interfaces )
		{
			const auto flags = iface.flags();
			if( flags.testFlag( QNetworkInterface::IsUp ) &&
				flags.testFlag( QNetworkInterface::IsRunning ) &&
				flags.testFlag( QNetworkInterface::IsLoopBack ) == false &&
				iface.addressEntries().isEmpty() == false &&
				( type == QNetworkInterface::Unknown || iface.type() == type ) )
			{
				return iface;
			}
		}
	}

	return {};
}



int AndroidNetworkFunctions::networkInterfaceSpeedInMBitPerSecond( const QNetworkInterface& networkInterface )
{
	Q_UNUSED(networkInterface)
	return 0;
}



// ---------------------------------------------------------------------------
// Service - there is no Veyon Service on Android

bool AndroidServiceFunctions::install( const QString& name, const QString& serviceFilePath,
									   StartMode startMode, const QString& displayName )
{
	Q_UNUSED(name)
	Q_UNUSED(serviceFilePath)
	Q_UNUSED(startMode)
	Q_UNUSED(displayName)
	return false;
}



bool AndroidServiceFunctions::setStartMode( const QString& name, StartMode startMode )
{
	Q_UNUSED(name)
	Q_UNUSED(startMode)
	return false;
}



bool AndroidServiceFunctions::runAsService( const QString& name, const ServiceEntryPoint& serviceEntryPoint )
{
	Q_UNUSED(name)
	Q_UNUSED(serviceEntryPoint)
	return false;
}



// ---------------------------------------------------------------------------
// Session

PlatformSessionFunctions::SessionUptime AndroidSessionFunctions::currentSessionUptime() const
{
	struct sysinfo info{};
	if( sysinfo( &info ) == 0 )
	{
		return static_cast<SessionUptime>( info.uptime );
	}

	return InvalidSessionUptime;
}



QString AndroidSessionFunctions::currentSessionHostName() const
{
	// Android always reports "localhost" as host name - the device model is
	// much more useful for identifying the Master on the client side
	const auto model = androidBuildField( "MODEL" );
	return model.isEmpty() ? QHostInfo::localHostName() : model;
}



// ---------------------------------------------------------------------------
// User

QString AndroidUserFunctions::queryCurrentUserProperty( UserProperty property )
{
	switch( property )
	{
	case UserProperty::LoginName:
		return QStringLiteral("android");
	case UserProperty::FullName:
	{
		const auto manufacturer = androidBuildField( "MANUFACTURER" );
		const auto model = androidBuildField( "MODEL" );
		if( model.startsWith( manufacturer, Qt::CaseInsensitive ) || manufacturer.isEmpty() )
		{
			return model;
		}
		return manufacturer.left(1).toUpper() + manufacturer.mid(1) + QLatin1Char(' ') + model;
	}
	case UserProperty::None:
		break;
	}

	return {};
}



bool AndroidUserFunctions::prepareLogon( const QString& username, const Password& password )
{
	Q_UNUSED(username)
	Q_UNUSED(password)
	return false;
}



bool AndroidUserFunctions::performLogon( const QString& username, const Password& password )
{
	Q_UNUSED(username)
	Q_UNUSED(password)
	return false;
}



bool AndroidUserFunctions::authenticate( const QString& username, const Password& password )
{
	// there are no local accounts to check against on Android
	Q_UNUSED(username)
	Q_UNUSED(password)
	return false;
}
