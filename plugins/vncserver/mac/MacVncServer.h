/*
 * MacVncServer.h - declaration of MacVncServer class
 *
 * Copyright (c) 2026 Veyon Community / macOS port
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
#include "VncServerPluginInterface.h"

struct MacVncScreen;

class MacVncServer : public QObject, VncServerPluginInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.MacVncServer")
	Q_INTERFACES(PluginInterface VncServerPluginInterface)
public:
	explicit MacVncServer( QObject* parent = nullptr );

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("9d3e4f21-6c8b-4a1d-9f2e-1a7b5c8d0e34") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 0, 1 );
	}

	QString name() const override
	{
		return QStringLiteral( "MacVncServer" );
	}

	QString description() const override
	{
		return tr( "Built-in VNC server for macOS (CoreGraphics screen capture + CGEvent input)" );
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

	QStringList supportedSessionTypes() const override
	{
		return { QStringLiteral("console") };
	}

	QWidget* configurationWidget() override
	{
		return nullptr;
	}

	void prepareServer() override;

	bool runServer( int serverPort, const Password& password ) override;

	int configuredServerPort() override
	{
		return -1;
	}

	Password configuredPassword() override
	{
		return {};
	}

private:
	static constexpr int DefaultCaptureIntervalMs = 50;

	bool initScreen( MacVncScreen* screen );
	bool initVncServer( int serverPort, const Password& password, MacVncScreen* screen );

	static void rfbLogNone( const char* format, ... );
	static void rfbLogDebug( const char* format, ... );

};
