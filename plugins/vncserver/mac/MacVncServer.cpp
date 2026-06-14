/*
 * MacVncServer.cpp - implementation of MacVncServer class
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

extern "C" {
#include "rfb/rfb.h"
}

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#include <CoreGraphics/CoreGraphics.h>

#include <QImage>
#include <QThread>

#include "MacVncServer.h"
#include "MacVncInput.h"
#include "MacScreenCapture.h"
#include "VeyonConfiguration.h"


struct MacVncScreen
{
	~MacVncScreen()
	{
		delete[] passwords[0];
	}

	rfbScreenInfoPtr rfbScreen{nullptr};
	std::array<char *, 2> passwords{};
	QImage framebuffer;
	QImage scratch;
	CGDirectDisplayID display{0};
};


// ---- libvncserver input callbacks ----------------------------------------

static void macKbdAddEvent( rfbBool down, rfbKeySym keySym, rfbClientPtr /*cl*/ )
{
	macVncInjectKey( down != FALSE, static_cast<uint32_t>( keySym ) );
}


static void macPtrAddEvent( int buttonMask, int x, int y, rfbClientPtr cl )
{
	macVncInjectPointer( buttonMask, x, y );
	rfbDefaultPtrAddEvent( buttonMask, x, y, cl );
}


static enum rfbNewClientAction macNewClientHook( rfbClientPtr cl )
{
	vInfo() << "MacVncServer: VNC client connected from"
			<< ( cl && cl->host ? cl->host : "?" );
	return RFB_CLIENT_ACCEPT;
}


// ---- plugin --------------------------------------------------------------

MacVncServer::MacVncServer( QObject* parent ) :
	QObject( parent )
{
}



void MacVncServer::prepareServer()
{
}



bool MacVncServer::runServer( int serverPort, const Password& password )
{
	if( VeyonCore::isDebugging() )
	{
		rfbLog = rfbLogDebug;
		rfbErr = rfbLogDebug;
	}
	else
	{
		rfbLog = rfbLogNone;
		rfbErr = rfbLogNone;
	}

	MacVncScreen screen;

	if( initScreen( &screen ) == false ||
		initVncServer( serverPort, password, &screen ) == false )
	{
		return false;
	}

	const size_t bufferBytes = static_cast<size_t>( screen.framebuffer.bytesPerLine() ) *
							   static_cast<size_t>( screen.framebuffer.height() );

	// Capture runs on its own thread so that slow frames (e.g. while the Screen
	// Recording permission is still missing, each capture blocks for its full
	// timeout) never stall the RFB event loop. The capture thread fills a shared
	// scratch buffer; the RFB loop copies it into the live framebuffer.
	std::atomic<bool> running{ true };
	std::atomic<bool> hasNewFrame{ false };
	std::mutex scratchMutex;
	QImage captured{ screen.framebuffer.width(), screen.framebuffer.height(), QImage::Format_RGB32 };
	captured.fill( Qt::black );

	std::thread captureThread( [&]() {
		QImage local{ screen.framebuffer.width(), screen.framebuffer.height(), QImage::Format_RGB32 };
		while( running.load() )
		{
			if( macScreenCaptureFrame( local ) )
			{
				std::lock_guard<std::mutex> lock( scratchMutex );
				std::memcpy( captured.bits(), local.bits(), bufferBytes );
				hasNewFrame.store( true );
			}
			else
			{
				// capture unavailable (e.g. permission missing) - avoid a busy loop
				std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
			}
		}
	} );

	while( rfbIsActive( screen.rfbScreen ) )
	{
		if( hasNewFrame.exchange( false ) )
		{
			std::lock_guard<std::mutex> lock( scratchMutex );
			if( std::memcmp( captured.bits(), screen.framebuffer.bits(), bufferBytes ) != 0 )
			{
				std::memcpy( screen.framebuffer.bits(), captured.bits(), bufferBytes );
				rfbMarkRectAsModified( screen.rfbScreen, 0, 0,
									   screen.rfbScreen->width, screen.rfbScreen->height );
			}
		}

		rfbProcessEvents( screen.rfbScreen, DefaultCaptureIntervalMs * 1000 );
	}

	running.store( false );
	captureThread.join();

	rfbShutdownServer( screen.rfbScreen, true );
	rfbScreenCleanup( screen.rfbScreen );
	macScreenCaptureCleanup();

	return true;
}



bool MacVncServer::initScreen( MacVncScreen* screen )
{
	screen->display = CGMainDisplayID();

	int width = 0;
	int height = 0;

	if( macScreenCaptureInit( screen->display, &width, &height ) == false ||
		width <= 0 || height <= 0 )
	{
		// Most likely the Screen Recording permission has not been granted yet.
		vCritical() << "MacVncServer: could not initialise screen capture. Grant the "
					   "Screen Recording permission to veyon-server in "
					   "System Settings > Privacy & Security > Screen Recording.";
		// fall back to the display's dimensions so the server still starts
		const auto bounds = CGDisplayBounds( screen->display );
		width = static_cast<int>( bounds.size.width );
		height = static_cast<int>( bounds.size.height );
		if( width <= 0 || height <= 0 )
		{
			return false;
		}
	}

	screen->framebuffer = QImage( width, height, QImage::Format_RGB32 );
	screen->framebuffer.fill( Qt::black );
	screen->scratch = QImage( width, height, QImage::Format_RGB32 );
	screen->scratch.fill( Qt::black );

	// map the RFB framebuffer (pixels) onto the display bounds (points)
	macVncInputInit( screen->display, CGDisplayBounds( screen->display ), width, height );

	return true;
}



bool MacVncServer::initVncServer( int serverPort, const Password& password, MacVncScreen* screen )
{
	auto rfbScreen = rfbGetScreen( nullptr, nullptr,
								   screen->framebuffer.width(), screen->framebuffer.height(),
								   8, 3, 4 );
	if( rfbScreen == nullptr )
	{
		return false;
	}

	screen->passwords[0] = qstrdup( password.toByteArray().constData() );

	rfbScreen->desktopName = "VeyonVNC";
	rfbScreen->frameBuffer = reinterpret_cast<char *>( screen->framebuffer.bits() );
	rfbScreen->port = serverPort;
	rfbScreen->ipv6port = serverPort;

	rfbScreen->authPasswdData = screen->passwords.data();
	rfbScreen->passwordCheck = rfbCheckPasswordByList;

	rfbScreen->serverFormat.redShift = 16;
	rfbScreen->serverFormat.greenShift = 8;
	rfbScreen->serverFormat.blueShift = 0;
	rfbScreen->serverFormat.redMax = 255;
	rfbScreen->serverFormat.greenMax = 255;
	rfbScreen->serverFormat.blueMax = 255;
	rfbScreen->serverFormat.trueColour = true;
	rfbScreen->serverFormat.bitsPerPixel = 32;

	rfbScreen->alwaysShared = true;
	rfbScreen->handleEventsEagerly = true;
	rfbScreen->deferUpdateTime = 5;

	rfbScreen->screenData = screen;
	rfbScreen->cursor = nullptr;

	rfbScreen->kbdAddEvent = macKbdAddEvent;
	rfbScreen->ptrAddEvent = macPtrAddEvent;
	rfbScreen->newClientHook = macNewClientHook;

	rfbInitServer( rfbScreen );

	rfbMarkRectAsModified( rfbScreen, 0, 0, rfbScreen->width, rfbScreen->height );

	screen->rfbScreen = rfbScreen;

	return true;
}



void MacVncServer::rfbLogDebug( const char* format, ... )
{
	va_list args;
	va_start( args, format );

	static constexpr int MaxMessageLength = 256;
	std::array<char, MaxMessageLength> message{};
	std::vsnprintf( message.data(), message.size(), format, args ); // Flawfinder: ignore

	va_end( args );

	vDebug() << message.data();
}



void MacVncServer::rfbLogNone( const char* format, ... )
{
	Q_UNUSED(format)
}
