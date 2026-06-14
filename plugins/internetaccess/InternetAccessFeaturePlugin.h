/*
 * InternetAccessFeaturePlugin.h - declaration of InternetAccessFeaturePlugin
 *
 * Copyright (c) 2026 Arunika / AruniControl
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

#include "Feature.h"
#include "FeatureProviderInterface.h"

class InternetAccessFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.InternetAccess")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit InternetAccessFeaturePlugin( QObject* parent = nullptr );
	~InternetAccessFeaturePlugin() override = default;

	enum Command
	{
		BlockInternet,
		AllowInternet,
	};

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("2b6e9c47-1a3d-4f58-8b0c-7d9e2f4a1c63") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("InternetAccessControl");
	}

	QString description() const override
	{
		return tr( "Block or allow internet access on selected computers." );
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

	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

private:
	bool applyInternetBlock( bool blocked );

	const Feature m_internetAccessFeature;
	const FeatureList m_features;

};
