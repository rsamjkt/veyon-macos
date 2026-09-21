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
#include <QElapsedTimer>

#include "MacScreenCapture.h"
#include "MacVncConfiguration.h"

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
	// how long to wait for the next captured frame; the wait ends early as soon
	// as a frame arrives, so this only bounds how long client input can sit
	// unhandled while nothing on screen moves
	static constexpr int FrameWaitMs = 10;

	// how long rfbProcessEvents() may block waiting for client input
	static constexpr int ClientPollIntervalMs = 2;

	// how long it may block while no client is connected at all
	static constexpr int IdlePollIntervalMs = 100;

	// how soon restarting a stopped capture stream is attempted, and how far
	// that delay is allowed to grow while it keeps failing
	static constexpr int CaptureRecoveryIntervalMs = 2000;
	static constexpr int MaxCaptureRecoveryIntervalMs = 30000;

	// Decide how the display is captured and start tracking the system cursor
	// when it is to be sent separately - the two go together because the cursor
	// may only be left out of the frames if we can read its shape ourselves.
	MacScreenCaptureOptions prepareCapture( CGDirectDisplayID display );
	bool initScreen( MacVncScreen* screen );
	void resizeFramebuffer( MacVncScreen* screen, int width, int height );
	void recoverCapture( MacVncScreen* screen );
	void backOffCaptureRecovery();
	bool initVncServer( int serverPort, const Password& password, MacVncScreen* screen );
	void updateCursor( MacVncScreen* screen );

	static void rfbLogNone( const char* format, ... );
	static void rfbLogDebug( const char* format, ... );

	MacVncConfiguration m_configuration;

	QElapsedTimer m_captureRecoveryTimer;
	int m_captureRecoveryDelay{CaptureRecoveryIntervalMs};

	// whether the cursor is sent as a VNC cursor shape instead of being part of
	// the captured pixels - decided at startup, see initScreen()
	bool m_remoteCursor{false};

};
