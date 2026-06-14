/*
 * NetworkDiscoveryPlugin.h - declaration of NetworkDiscoveryPlugin class
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

#include "PluginInterface.h"
#include "NetworkObjectDirectoryPluginInterface.h"

// clazy:excludeall=copyable-polymorphic

class NetworkDiscoveryPlugin : public QObject,
		PluginInterface,
		NetworkObjectDirectoryPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.NetworkDiscovery")
	Q_INTERFACES(PluginInterface NetworkObjectDirectoryPluginInterface)
public:
	explicit NetworkDiscoveryPlugin( QObject* parent = nullptr );
	~NetworkDiscoveryPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("3c5e9a14-2b7d-4e6f-8a1c-9d0f2e4b6c81") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "NetworkDiscovery" );
	}

	QString description() const override
	{
		return tr( "Automatically discovers computers running AruniControl Server on the local network" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Arunika" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "Arunika" );
	}

	QString directoryName() const override
	{
		return tr( "Network discovery" );
	}

	NetworkObjectDirectory* createNetworkObjectDirectory( QObject* parent ) override;

};
