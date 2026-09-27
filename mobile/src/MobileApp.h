/*
 * MobileApp.h - backend of the AruniControl Mobile UI
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

#pragma once

#include <QObject>
#include <QUrl>
#include <QVariant>

#include "ComputerControlInterface.h"
#include "FeatureProviderInterface.h"

#include "AccessLogController.h"
#include "AppMonitorController.h"
#include "AppUpdater.h"
#include "BroadcastController.h"
#include "ChatController.h"
#include "ComputerGridModel.h"
#include "GatewayManager.h"
#include "SiteFilterController.h"
#include "VoiceController.h"
#include "VpnController.h"

class VeyonMaster;

// Everything the QML UI needs from the Master: authentication setup, the
// computer list, rooms/computers management and running features. Features
// are run through FeatureProviderInterface::controlFeature(), i.e. without the
// desktop dialogs the plugins would otherwise show - the UI asks for all
// parameters itself.
class MobileApp : public QObject
{
	Q_OBJECT
	Q_PROPERTY(ComputerGridModel* computers READ computers CONSTANT)
	Q_PROPERTY(VpnController* vpn READ vpn CONSTANT)
	Q_PROPERTY(GatewayManager* gateways READ gateways CONSTANT)
	Q_PROPERTY(ChatController* chat READ chat CONSTANT)
	Q_PROPERTY(VoiceController* voice READ voice CONSTANT)
	Q_PROPERTY(AppMonitorController* appMonitor READ appMonitor CONSTANT)
	Q_PROPERTY(BroadcastController* broadcast READ broadcast CONSTANT)
	Q_PROPERTY(SiteFilterController* siteFilter READ siteFilter CONSTANT)
	Q_PROPERTY(AccessLogController* accessLog READ accessLog CONSTANT)
	Q_PROPERTY(AppUpdater* updater READ updater CONSTANT)
	Q_PROPERTY(bool authenticated READ isAuthenticated NOTIFY authenticationChanged)
	Q_PROPERTY(QString authMethod READ authMethod NOTIFY authenticationChanged)
	Q_PROPERTY(QString authName READ authName NOTIFY authenticationChanged)
	Q_PROPERTY(QVariantList rooms READ rooms NOTIFY roomsChanged)
	Q_PROPERTY(QString networkAddress READ networkAddress NOTIFY networkChanged)
	Q_PROPERTY(QString version READ version CONSTANT)
	Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
	Q_PROPERTY(bool isMobile READ isMobile CONSTANT)
	Q_PROPERTY(QString screenshotDirectory READ screenshotDirectory CONSTANT)
	Q_PROPERTY(QString collectedFilesDirectory READ collectedFilesDirectory CONSTANT)
public:
	explicit MobileApp( VeyonMaster* master, QObject* parent = nullptr );
	~MobileApp() override;

	// first-run defaults - must run before VeyonMaster is created
	static void applyDefaults();

	ComputerGridModel* computers() const
	{
		return m_computers;
	}

	VpnController* vpn() const
	{
		return m_vpn;
	}

	AppUpdater* updater() const
	{
		return m_updater;
	}

	GatewayManager* gateways() const
	{
		return m_gateways;
	}

	ChatController* chat() const
	{
		return m_chat;
	}

	VoiceController* voice() const
	{
		return m_voice;
	}

	AppMonitorController* appMonitor() const
	{
		return m_appMonitor;
	}

	BroadcastController* broadcast() const
	{
		return m_broadcast;
	}

	SiteFilterController* siteFilter() const
	{
		return m_siteFilter;
	}

	AccessLogController* accessLog() const
	{
		return m_accessLog;
	}

	bool isAuthenticated() const;
	QString authMethod() const;
	QString authName() const;

	QVariantList rooms() const;
	QString networkAddress() const;
	QString version() const;
	QString deviceName() const;
	QString screenshotDirectory() const;
	QString collectedFilesDirectory() const;

	bool isMobile() const
	{
#ifdef Q_OS_ANDROID
		return true;
#else
		return false;
#endif
	}

	// --- authentication
	Q_INVOKABLE QString suggestKeyName( const QUrl& fileUrl ) const;
	Q_INVOKABLE QString displayName( const QUrl& fileUrl ) const;
	Q_INVOKABLE QString importKeyFile( const QUrl& fileUrl, const QString& keyName );
	Q_INVOKABLE QString importKeyText( const QString& pem, const QString& keyName );
	Q_INVOKABLE QString useLogon( const QString& username, const QString& password, bool remember );
	Q_INVOKABLE void signOut();

	// --- rooms & computers (built-in directory, merged with network discovery)
	Q_INVOKABLE QString addRoom( const QString& name );
	Q_INVOKABLE void renameRoom( const QString& roomUid, const QString& name );
	Q_INVOKABLE void removeRoom( const QString& roomUid );
	Q_INVOKABLE QString addComputer( const QString& roomUid, const QString& name, const QString& host, const QString& mac );
	Q_INVOKABLE void removeComputer( const QString& computerUid );
	// rooms and computers from a CSV file (columns Ruangan, Nama, Alamat IP, MAC)
	Q_INVOKABLE QVariantMap importComputers( const QUrl& fileUrl );
	Q_INVOKABLE void refreshComputers();

	// --- features; an empty uid list means "all visible computers"
	Q_INVOKABLE bool hasFeature( const QString& name ) const;
	Q_INVOKABLE bool runFeature( const QString& name, bool start, const QVariantMap& arguments, const QStringList& uids );
	Q_INVOKABLE void lockScreens( bool lock, const QStringList& uids );
	Q_INVOKABLE void sendMessage( const QString& title, const QString& text, const QStringList& uids );
	Q_INVOKABLE void powerAction( const QString& action, const QStringList& uids, int delaySeconds = 0 );
	Q_INVOKABLE void openWebsite( const QString& url, const QStringList& uids );
	Q_INVOKABLE void startApplication( const QString& command, const QStringList& uids );
	Q_INVOKABLE void setInternetBlocked( bool blocked, const QStringList& uids );
	Q_INVOKABLE void setAudioMuted( bool muted, const QStringList& uids );
	// exam mode (ExamMode plugin): settings { sites, url, blockInternet, closeApps, lockKeys }
	Q_INVOKABLE QVariantMap examSettings() const;
	Q_INVOKABLE void startExam( const QVariantMap& settings, const QStringList& uids );
	Q_INVOKABLE void endExam( const QStringList& uids );
	Q_INVOKABLE int saveScreenshots( const QStringList& uids );
	Q_INVOKABLE void lockInput( bool lock, const QStringList& uids );
	Q_INVOKABLE void loginUser( const QString& username, const QString& password, const QStringList& uids );
	Q_INVOKABLE QString shareScreen( const QString& sourceUid, bool fullScreen, const QStringList& uids );
	Q_INVOKABLE void stopDemo( const QStringList& uids );
	Q_INVOKABLE QString sendFiles( const QList<QUrl>& files, const QStringList& uids );
	Q_INVOKABLE void collectFiles( const QStringList& uids );
	Q_INVOKABLE QStringList collectedFiles() const;
	Q_INVOKABLE int targetCount( const QStringList& uids ) const;

	Q_INVOKABLE void shareFile( const QString& path );
	Q_INVOKABLE QStringList screenshots() const;

	// development hooks for desktop builds (always empty on Android)
	Q_INVOKABLE QString devOption( const QString& name ) const;

	// "arunicontrol://pair?c=..." links (QDesktopServices URL handler)
	Q_INVOKABLE void handleUrl( const QUrl& url );
	Q_INVOKABLE bool deleteFile( const QString& path );

Q_SIGNALS:
	void authenticationChanged();
	void roomsChanged();
	void networkChanged();
	void notify( const QString& message, const QString& kind );
	void gatewayAdded( const QString& name );

private:
	ComputerControlInterfaceList targets( const QStringList& uids ) const;
	Feature featureByName( const QString& name ) const;
	void reconnectAll();
	void checkAllLocations();

	QJsonArray directoryObjects() const;
	void setDirectoryObjects( const QJsonArray& objects );

	void saveConfiguration();
	void publishGatewayHosts();
	QStringList managedLocationUids() const;

	VeyonMaster* m_master;
	ComputerGridModel* m_computers;
	VpnController* m_vpn;
	GatewayManager* m_gateways;
	AppUpdater* m_updater;
	ChatController* m_chat;
	VoiceController* m_voice;
	AppMonitorController* m_appMonitor;
	BroadcastController* m_broadcast;
	SiteFilterController* m_siteFilter;
	AccessLogController* m_accessLog;

};
