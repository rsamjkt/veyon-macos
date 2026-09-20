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

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

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


// ---- framebuffer updates -------------------------------------------------

namespace {

constexpr int TileSize = 64;


bool tileChanged( const uchar* framebuffer, int framebufferStride,
				  const QImage& source, int x, int y, int tileWidth, int tileHeight )
{
	const size_t offset = static_cast<size_t>( x ) * 4;
	const size_t rowBytes = static_cast<size_t>( tileWidth ) * 4;

	for( int row = 0; row < tileHeight; ++row )
	{
		if( std::memcmp( framebuffer + static_cast<size_t>( y+row )*framebufferStride + offset,
						 source.constScanLine( y+row ) + offset, rowBytes ) != 0 )
		{
			return true;
		}
	}

	return false;
}


// Copy only those parts of `source` that actually differ from the live
// framebuffer and mark just those as modified. Marking the whole screen on
// every frame - which is what a naive implementation does - makes
// libvncserver re-encode and re-transmit the entire desktop for something as
// small as a blinking cursor, which is what made the remote view stutter.
//
// `region` bounds the search to what ScreenCaptureKit reported as changed; the
// per-tile comparison below still decides what is actually sent, so an overly
// generous region only costs a little extra comparing.
void updateChangedTiles( rfbScreenInfoPtr rfbScreen, uchar* framebuffer, int framebufferStride,
						 const QImage& source, const MacScreenCaptureRegion& region )
{
	const int width = std::min( rfbScreen->width, source.width() );
	const int height = std::min( rfbScreen->height, source.height() );

	// snap to the tile grid so that a tile always covers the same pixels
	const int xStart = std::max( 0, region.x - region.x % TileSize );
	const int yStart = std::max( 0, region.y - region.y % TileSize );
	const int xEnd = std::min( width, region.x + region.width );
	const int yEnd = std::min( height, region.y + region.height );

	for( int y = yStart; y < yEnd; y += TileSize )
	{
		const int tileHeight = std::min( TileSize, height - y );

		// consecutive changed tiles are merged into a single rectangle so that
		// the update stays well below rfbScreen->maxRectsPerUpdate
		int runStart = -1;
		int runEnd = -1;

		const auto flushRun = [&]() {
			const size_t offset = static_cast<size_t>( runStart ) * 4;
			const size_t rowBytes = static_cast<size_t>( runEnd - runStart ) * 4;

			for( int row = 0; row < tileHeight; ++row )
			{
				std::memcpy( framebuffer + static_cast<size_t>( y+row )*framebufferStride + offset,
							 source.constScanLine( y+row ) + offset, rowBytes );
			}

			rfbMarkRectAsModified( rfbScreen, runStart, y, runEnd, y+tileHeight );
			runStart = -1;
		};

		for( int x = xStart; x < xEnd; x += TileSize )
		{
			const int tileWidth = std::min( TileSize, width - x );

			if( tileChanged( framebuffer, framebufferStride, source, x, y, tileWidth, tileHeight ) )
			{
				if( runStart < 0 )
				{
					runStart = x;
				}
				runEnd = x + tileWidth;
			}
			else if( runStart >= 0 )
			{
				flushRun();
			}
		}

		if( runStart >= 0 )
		{
			flushRun();
		}
	}
}

} // namespace


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

	// ScreenCaptureKit delivers frames asynchronously on its own queue, so the
	// RFB loop only ever picks up whatever has already been captured - it never
	// waits for the capture hardware. When nothing on screen changes no frame
	// arrives at all and this loop costs virtually nothing.
	// carries the previous frame - macScreenCaptureFrame() only writes the part
	// that changed
	QImage captured{ screen.framebuffer.width(), screen.framebuffer.height(), QImage::Format_RGB32 };
	captured.fill( Qt::black );

	auto* framebuffer = reinterpret_cast<uchar *>( screen.framebuffer.bits() );
	const auto framebufferStride = static_cast<int>( screen.framebuffer.bytesPerLine() );

	while( rfbIsActive( screen.rfbScreen ) )
	{
		MacScreenCaptureRegion changedRegion;

		if( macScreenCaptureFrame( captured, &changedRegion ) )
		{
			updateChangedTiles( screen.rfbScreen, framebuffer, framebufferStride,
								captured, changedRegion );
		}

		rfbProcessEvents( screen.rfbScreen, PollIntervalMs * 1000 );
	}

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

	// updateChangedTiles() produces one rectangle per run of changed tiles;
	// once a client exceeds this limit libvncserver falls back to sending the
	// bounding box of everything that changed, which is exactly what we are
	// trying to avoid
	rfbScreen->maxRectsPerUpdate = 200;

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
