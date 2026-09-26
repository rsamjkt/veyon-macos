/*
 * SiteFilterFeaturePlugin.h - block websites on selected computers
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

#include <QStringList>

#include "Feature.h"
#include "FeatureProviderInterface.h"

// Blocks a list of websites (domains) on the selected computers.
//
// Protocol (feature "SiteFilter", uid below):
//   master -> server  SetSites  { Sites: QStringList }   (empty list = unblock)
//   master -> server  Query
//   server -> master  Status    { Sites: QStringList, Supported: bool, Error: QString }
//                     (reply to SetSites and Query)
// From code (e.g. the app): controlFeature( uid, Start, { "sites": QStringList } ),
// controlFeature( uid, Stop, {} ) unblocks, Operation::Initialize queries.
//
// Windows: a marked block in the hosts file (0.0.0.0 domain / www. / m.) plus
// browser policies that turn off DNS over HTTPS (Chrome, Edge, Firefox), which
// would otherwise bypass the hosts file. macOS: the hosts file needs admin
// rights, the user of the computer is asked for them.
class SiteFilterFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.SiteFilter")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit SiteFilterFeaturePlugin( QObject* parent = nullptr );
	~SiteFilterFeaturePlugin() override = default;

	enum Command
	{
		SetSites,
		Query,
		Status,
	};

	enum class Argument
	{
		Sites,
		Supported,
		Error,
	};

	static constexpr auto FeatureUid = "c3a5e2d1-7b4f-4e8a-9d61-2f0b8e7c4a15";

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("8a47d1e3-2c96-4f05-b7e8-5d31c0a9f624") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("SiteFilter");
	}

	QString description() const override
	{
		return tr( "Block websites on selected computers" );
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

	// "https://www.YouTube.com/watch" -> "youtube.com", empty if invalid
	static QString normalizedDomain( const QString& text );
	static QStringList normalizedDomains( const QStringList& texts );

	// ready-made lists (name -> domains)
	static QList<QPair<QString, QStringList>> presets();

	// last status reported by each computer (master side)
	QStringList sitesOf( const ComputerControlInterface::Pointer& computerControlInterface ) const;

Q_SIGNALS:
	void statusReceived( ComputerControlInterface::Pointer computerControlInterface,
						 const QStringList& sites, bool supported, const QString& error );

private:
	bool applySites( const QStringList& sites, QString& error );
	static QString statePath();
	static QStringList loadSites();
	static void saveSites( const QStringList& sites );

	const Feature m_siteFilterFeature;
	const FeatureList m_features;
	QMap<ComputerControlInterface*, QStringList> m_reportedSites;

};
