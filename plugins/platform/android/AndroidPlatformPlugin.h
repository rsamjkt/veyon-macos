/*
 * AndroidPlatformPlugin.h - declaration of AndroidPlatformPlugin class
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

#include "PluginInterface.h"
#include "PlatformPluginInterface.h"
#include "AndroidPlatformFunctions.h"

// clazy:excludeall=copyable-polymorphic

class AndroidPlatformPlugin : public QObject, PlatformPluginInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.AndroidPlatform")
	Q_INTERFACES(PluginInterface PlatformPluginInterface)
public:
	AndroidPlatformPlugin( QObject* parent = nullptr );
	~AndroidPlatformPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("6d1f4a52-3b7e-4c09-a8e1-5f2c9b7d0e41") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 0, 1 );
	}

	QString name() const override
	{
		return QStringLiteral( "AndroidPlatformPlugin" );
	}

	QString description() const override
	{
		return tr( "Plugin implementing abstract functions for the Android platform" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "AruniControl Community" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "AruniControl Community" );
	}

	Plugin::Flags flags() const override
	{
		return Plugin::ProvidesDefaultImplementation;
	}

	PlatformCoreFunctions& coreFunctions() override
	{
		return m_androidCoreFunctions;
	}

	PlatformFilesystemFunctions& filesystemFunctions() override
	{
		return m_androidFilesystemFunctions;
	}

	PlatformInputDeviceFunctions& inputDeviceFunctions() override
	{
		return m_androidInputDeviceFunctions;
	}

	PlatformNetworkFunctions& networkFunctions() override
	{
		return m_androidNetworkFunctions;
	}

	PlatformServiceFunctions& serviceFunctions() override
	{
		return m_androidServiceFunctions;
	}

	PlatformSessionFunctions& sessionFunctions() override
	{
		return m_androidSessionFunctions;
	}

	PlatformUserFunctions& userFunctions() override
	{
		return m_androidUserFunctions;
	}

private:
	AndroidCoreFunctions m_androidCoreFunctions{};
	AndroidFilesystemFunctions m_androidFilesystemFunctions{};
	AndroidInputDeviceFunctions m_androidInputDeviceFunctions{};
	AndroidNetworkFunctions m_androidNetworkFunctions{};
	AndroidServiceFunctions m_androidServiceFunctions{};
	AndroidSessionFunctions m_androidSessionFunctions{};
	AndroidUserFunctions m_androidUserFunctions{};

};
