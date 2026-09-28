/*
 * LabCleanFeaturePlugin.h - clean student accounts at every restart
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

#include <QJsonObject>

#include "Feature.h"
#include "FeatureProviderInterface.h"

// Lab clean mode: at every start of the computer the contents of chosen
// folders (Desktop, Downloads, Documents, ...) of chosen accounts (e.g.
// "siswa") are MOVED to %GLOBALAPPDATA%/labclean/<time>/ and deleted from
// there after some days - students always start with a clean account and
// nothing is lost by mistake. Accounts of administrators are never touched;
// programs and settings are not reset (no disk freeze).
//
// Windows: runs in the AruniControl service at boot (before anybody logs on);
// fast startup is switched off while enabled, so "shut down" also cleans.
// macOS: runs when the listed user logs on (server start).
//
// Protocol (feature "LabClean", uid below, permission "commands"):
//   master -> server  Configure { Settings: JSON }   ({} = off)
//   master -> server  CleanNow
//   master -> server  Query
//   server -> master  Status    { Settings: JSON, LastClean: QString, Moved: int, Error: QString }
class LabCleanFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.LabClean")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit LabCleanFeaturePlugin( QObject* parent = nullptr );
	~LabCleanFeaturePlugin() override = default;

	enum Command
	{
		Configure,
		CleanNow,
		Query,
		Status,
	};

	enum class Argument
	{
		Settings,
		LastClean,
		Moved,
		Error,
	};

	struct Settings
	{
		bool enabled{false};
		QStringList users;
		QStringList folders;
		int keepDays{7};
	};

	static constexpr auto FeatureUid = "f2a9c6e1-4b73-4d08-8e5a-1c7b3d9f6e42";

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("8c4d2f61-7a3e-4b95-9e08-5d1b6c3a7f24") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("LabClean");
	}

	QString description() const override
	{
		return tr( "Clean the student accounts at every restart" );
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

	static QStringList defaultFolders();
	static QString folderDisplayName( const QString& folder );

	static Settings loadSettings();
	static QJsonObject toJson( const Settings& settings );
	static Settings fromJson( const QJsonObject& json );

	// moves the files away, returns how many entries were moved
	static int clean( const Settings& settings, bool onlyOncePerBoot, QString& error );

Q_SIGNALS:
	void statusReceived( ComputerControlInterface::Pointer computer, const QJsonObject& settings,
						 const QString& lastClean, int moved, const QString& error );

private:
	static QString statePath();
	static QString quarantineDirectory();
	static void saveSettings( const Settings& settings );
	static bool isAdministrator( const QString& user );
	static QString profilePath( const QString& user );
	static QString bootId();
	static void setFastStartupDisabled( bool disabled );
	static void prune( int keepDays );

	const Feature m_feature;
	const FeatureList m_features;

};
