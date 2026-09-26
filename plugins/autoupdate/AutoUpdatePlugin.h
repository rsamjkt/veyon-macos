/*
 * AutoUpdatePlugin.h - automatic updates of AruniControl
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

#include "ConfigurationPagePluginInterface.h"
#include "PluginInterface.h"

class Updater;

class AutoUpdatePlugin : public QObject, PluginInterface, ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.AutoUpdate")
	Q_INTERFACES(PluginInterface ConfigurationPagePluginInterface)
public:
	explicit AutoUpdatePlugin( QObject* parent = nullptr );
	~AutoUpdatePlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("5d2f8a61-3c47-4b9e-a0d1-7e6c94b2f835") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "AutoUpdate" );
	}

	QString description() const override
	{
		return tr( "Keeps AruniControl up to date automatically" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Arunika" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "AruniControl Community" );
	}

	ConfigurationPage* createConfigurationPage() override;

private:
	Updater* m_updater{nullptr};

};
