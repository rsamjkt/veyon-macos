/*
 * AccessLogFeaturePlugin.h - who accessed this computer, when and how
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

#include <QJsonArray>

#include "Feature.h"
#include "FeatureProviderInterface.h"

// Serves the access log of a computer (written by veyon-server, see
// core/src/AccessLog) to Masters, the app and the Aruni Gateway collector.
//
// Protocol (feature "AccessLog", uid below):
//   master -> server  Query    { Since: QString (ISO date/time, UTC, optional), Limit: int }
//   server -> master  Entries  { Entries: QByteArray (JSON array, oldest first) }
// Each entry: { t: ISO UTC, e: event, host, user, feature?, seconds? } with
// e = "connected" | "disconnected" | "auth_failed" | "access_denied" | "feature"
// From code: controlFeature( uid, Initialize, { "since": iso, "limit": n } ).
class AccessLogFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.AccessLog")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit AccessLogFeaturePlugin( QObject* parent = nullptr );
	~AccessLogFeaturePlugin() override = default;

	enum Command
	{
		Query,
		Entries,
	};

	enum class Argument
	{
		Since,
		Limit,
		Entries,
	};

	static constexpr auto FeatureUid = "e81f4c27-5d3a-4b96-8c0e-3a7d9f21b640";

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("4e0b6c95-a371-4d28-9f1e-6b85d2c7a403") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("AccessLog");
	}

	QString description() const override
	{
		return tr( "Show who accessed the selected computers" );
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

Q_SIGNALS:
	void entriesReceived( ComputerControlInterface::Pointer computerControlInterface, const QJsonArray& entries );

private:
	const Feature m_accessLogFeature;
	const FeatureList m_features;

};
