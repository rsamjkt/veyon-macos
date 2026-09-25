/*
 * AndroidPlatformFunctions.h - platform function groups for Android
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

#pragma once

// On Android AruniControl only runs as a Master (the controlling side), so the
// functions needed by a controlled client - services, logon, input blocking,
// screen savers - are intentionally no-ops.

#include "KeyboardShortcutTrapper.h"
#include "PlatformCoreFunctions.h"
#include "PlatformFilesystemFunctions.h"
#include "PlatformInputDeviceFunctions.h"
#include "PlatformNetworkFunctions.h"
#include "PlatformServiceFunctions.h"
#include "PlatformSessionFunctions.h"
#include "PlatformUserFunctions.h"

// clazy:excludeall=copyable-polymorphic

class AndroidCoreFunctions : public PlatformCoreFunctions
{
public:
	bool applyConfiguration() override { return true; }
	bool prepareSessionBusAccess() override { return true; }

	void initNativeLoggingSystem( const QString& appName ) override;
	void writeToNativeLoggingSystem( const QString& message, Logger::LogLevel loglevel ) override;

	QObject* notifyOnStandardInputReadyRead( const NotifierCallback& callback ) override;

	void reboot() override {}
	void powerDown( bool installUpdates ) override { Q_UNUSED(installUpdates) }

	void raiseWindow( QWidget* widget, bool stayOnTop ) override;

	void disableScreenSaver() override {}
	void restoreScreenSaverSettings() override {}

	void setSystemUiState( bool enabled ) override { Q_UNUSED(enabled) }

	QString activeDesktopName() override { return {}; }

	bool isRunningAsAdmin() const override { return false; }
	bool runProgramAsAdmin( const QString& program, const QStringList& parameters ) override;

	bool runProgramAsUser( const QString& program, const QStringList& parameters,
						   const QString& username,
						   const QString& desktop,
						   const QByteArray& stdInData ) override;

	QString genericUrlHandler() const override { return {}; }

	QString queryDisplayDeviceName( const QScreen& screen ) const override;

	QString getApplicationName( ProcessId processId ) const override;

private:
	QByteArray m_loggingTag{"AruniControl"};

};



class AndroidFilesystemFunctions : public PlatformFilesystemFunctions
{
public:
	QString personalAppDataPath() const override;
	QString globalAppDataPath() const override;
	QString globalTempPath() const override;

	QString fileOwnerGroup( const QString& filePath ) override;
	bool setFileOwnerGroup( const QString& filePath, const QString& ownerGroup ) override;
	bool setFileOwnerGroupPermissions( const QString& filePath, QFile::Permissions permissions ) override;

	bool openFileSafely( QFile* file, QFile::OpenMode openMode, QFile::Permissions permissions ) override;

	PlatformCoreFunctions::ProcessId findFileLockingProcess( const QString& filePath ) const override;

};



class AndroidKeyboardShortcutTrapper : public KeyboardShortcutTrapper
{
	Q_OBJECT
public:
	explicit AndroidKeyboardShortcutTrapper( QObject* parent = nullptr ) :
		KeyboardShortcutTrapper( parent )
	{
	}

	void setEnabled( bool on ) override
	{
		Q_UNUSED(on)
	}

};



class AndroidInputDeviceFunctions : public PlatformInputDeviceFunctions
{
public:
	void enableInputDevices() override {}
	void disableInputDevices() override {}

	KeyboardShortcutTrapper* createKeyboardShortcutTrapper( QObject* parent ) override
	{
		return new AndroidKeyboardShortcutTrapper( parent );
	}

};



class AndroidNetworkFunctions : public PlatformNetworkFunctions
{
public:
	PingResult ping( const QString& hostAddress ) override;
	bool configureFirewallException( const QString& applicationPath, const QString& description, bool enabled ) override;

	bool configureSocketKeepalive( Socket socket, bool enabled, int idleTime, int interval, int probes ) override;

	QNetworkInterface defaultRouteNetworkInterface() override;
	int networkInterfaceSpeedInMBitPerSecond( const QNetworkInterface& networkInterface ) override;

};



class AndroidServiceFunctions : public PlatformServiceFunctions
{
public:
	QString veyonServiceName() const override { return QStringLiteral("AruniControl"); }

	bool isRegistered( const QString& name ) override { Q_UNUSED(name) return false; }
	bool isRunning( const QString& name ) override { Q_UNUSED(name) return false; }
	bool start( const QString& name ) override { Q_UNUSED(name) return false; }
	bool stop( const QString& name ) override { Q_UNUSED(name) return false; }
	bool install( const QString& name, const QString& serviceFilePath,
				  StartMode startMode, const QString& displayName ) override;
	bool uninstall( const QString& name ) override { Q_UNUSED(name) return false; }
	bool setStartMode( const QString& name, StartMode startMode ) override;
	bool runAsService( const QString& name, const ServiceEntryPoint& serviceEntryPoint ) override;
	void manageServerInstances() override {}

};



class AndroidSessionFunctions : public PlatformSessionFunctions
{
public:
	SessionId currentSessionId() override { return DefaultSessionId; }

	SessionUptime currentSessionUptime() const override;
	QString currentSessionClientAddress() const override { return {}; }
	QString currentSessionClientName() const override { return {}; }
	QString currentSessionHostName() const override;

	QString currentSessionType() const override { return QStringLiteral("android"); }
	bool currentSessionHasUser() const override { return true; }

	EnvironmentVariables currentSessionEnvironmentVariables() const override { return {}; }
	QVariant querySettingsValueInCurrentSession( const QString& key ) const override
	{
		Q_UNUSED(key)
		return {};
	}

};



class AndroidUserFunctions : public PlatformUserFunctions
{
public:
	QString queryCurrentUserProperty( UserProperty property ) override;

	QStringList userGroups( bool queryDomainGroups ) override { Q_UNUSED(queryDomainGroups) return {}; }
	QStringList groupsOfUser( const QString& username, bool queryDomainGroups ) override
	{
		Q_UNUSED(username)
		Q_UNUSED(queryDomainGroups)
		return {};
	}
	QString userGroupSecurityIdentifier( const QString& groupName ) override { Q_UNUSED(groupName) return {}; }

	bool isAnyUserLoggedOn() override { return true; }

	bool prepareLogon( const QString& username, const Password& password ) override;
	bool performLogon( const QString& username, const Password& password ) override;
	void logoff() override {}

	bool authenticate( const QString& username, const Password& password ) override;

};
