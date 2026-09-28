/*
 * ExamModeFeaturePlugin.h - one-click exam mode
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

#include <QHash>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "Feature.h"
#include "FeatureProviderInterface.h"

// One-click exam mode for the selected computers:
// - internet only for the exam (CBT) websites - everything else outbound is
//   blocked, the office network stays reachable
// - forbidden applications (chat, remote help, games...) are closed and kept
//   closed while the exam runs
// - the system keyboard shortcuts (Windows key, Alt+Tab, Alt+F4, Ctrl+Esc,
//   Task Manager) are locked
// - optionally the exam website is opened
// Stopping the mode undoes all of it. The mode survives a restart of the
// computer (state in %GLOBALAPPDATA%/exammode.json).
//
// Protocol (feature "ExamMode", uid below):
//   master -> server  Start  { Sites: QStringList, Apps: QStringList, LockKeys: bool,
//                              Url: QString, BlockInternet: bool, Kiosk: bool }
//   master -> server  Stop
//   master -> server  Query
//   server -> master  Status { Active: bool, Error: QString }  (reply to all three)
//   server -> system worker   StartWorker { LockKeys } / StopWorker
//   server -> session worker  StartBrowser { Url, Kiosk } / StopBrowser  (BrowserFeatureUid)
// Kiosk: Edge (or Chrome) full screen without address bar, started again if
// closed, until the exam ends.
// From code (the app, schedules): controlFeature( uid, Start, { "sites", "apps",
// "lockKeys", "url", "blockInternet" } ) and controlFeature( uid, Stop, {} ).
class ExamModeFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.ExamMode")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit ExamModeFeaturePlugin( QObject* parent = nullptr );
	~ExamModeFeaturePlugin() override = default;

	enum Command
	{
		Start,
		Stop,
		Query,
		Status,
		StartWorker,
		StopWorker,
		StartBrowser,
		StopBrowser,
	};

	enum class Argument
	{
		Sites,
		Apps,
		LockKeys,
		Url,
		BlockInternet,
		Active,
		Error,
		Kiosk,
	};

	static constexpr auto FeatureUid = "9b2e6c41-8d7a-4f35-a0c9-4e1b7d3f5a28";
	// the worker that opens the exam website runs with the rights of the
	// logged-on user, not as the system like the keyboard lock
	static constexpr auto BrowserFeatureUid = "0e7c4b92-5f13-4a68-b9d2-6c1a8e3f5b70";

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("5c8f1a36-2e9d-4b70-9f14-7a3c6e0d8b52") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ExamMode");
	}

	QString description() const override
	{
		return tr( "One-click exam mode for selected computers" );
	}

	QString vendor() const override
	{
		return QStringLiteral("Arunika");
	}

	QString copyright() const override
	{
		return QStringLiteral("Arunika");
	}

	const FeatureList& featureList() const override;

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	// applications closed by default (chat, video calls, remote help, games)
	static QStringList defaultApps();

	// domains the computers always need (AruniControl relay and updates)
	static QStringList requiredDomains();

Q_SIGNALS:
	void statusReceived( ComputerControlInterface::Pointer computerControlInterface, bool active, const QString& error );

private:
	struct Settings
	{
		bool active{false};
		QStringList sites;
		QStringList apps;
		bool lockKeys{true};
		QString url;
		bool blockInternet{true};
		bool kiosk{false};
	};

	void initServer( VeyonServerInterface& server );
	bool activate( VeyonServerInterface& server, const Settings& settings, QString& error, bool openWebsite );
	void deactivate( VeyonServerInterface& server, QString& error );
	void startWorker( VeyonServerInterface& server, bool openWebsite );
	void launchKioskBrowser();
	static QStringList kioskBrowser( const QString& url );
	bool stillActive();
	void closeApps();
	void resolveSites();
	bool applyFirewall( bool on, QString& error );
	void setTaskManagerLocked( bool locked );

	static QString statePath();
	static Settings loadSettings();
	static void saveSettings( const Settings& settings );

	const Feature m_examModeFeature;
	const Feature m_examBrowserFeature;
	const FeatureList m_features;

	// server side
	bool m_serverInitialized{false};
	Settings m_settings;
	QSet<QString> m_allowedAddresses;
	QTimer m_enforceTimer;
	QTimer m_resolveTimer;
	int m_pendingLookups{0};
	QString m_lastError;

	// worker side
	QObject* m_trapper{nullptr};
	QPointer<QProcess> m_browser;
	QString m_kioskUrl;
	bool m_kioskActive{false};
	QList<qint64> m_browserStarts;

};
