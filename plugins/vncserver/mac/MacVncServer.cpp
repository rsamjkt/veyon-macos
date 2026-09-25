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
#include <cstdlib>
#include <cstring>

#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>

#include <QImage>
#include <QThread>

#include "MacVncServer.h"
#include "MacVncCursor.h"
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

	// what the clients see
	QImage framebuffer;

	// the most recent capture; carries the previous frame between iterations
	// because macScreenCaptureFrame() only writes the part that changed
	QImage captured;

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

// The framebuffer is a QImage::Format_RGB32, i.e. 0xffRRGGBB in a native-endian
// word, which puts blue in the first byte on a little-endian machine.
// LibVNCServer's rfbInitServerFormat() assumes the opposite order, so the
// shifts have to be spelled out - and spelled out again after every
// rfbNewFramebuffer(), which silently resets them.
void applyServerFormat( rfbScreenInfoPtr rfbScreen )
{
	rfbScreen->serverFormat.redShift = 16;
	rfbScreen->serverFormat.greenShift = 8;
	rfbScreen->serverFormat.blueShift = 0;
	rfbScreen->serverFormat.redMax = 255;
	rfbScreen->serverFormat.greenMax = 255;
	rfbScreen->serverFormat.blueMax = 255;
	rfbScreen->serverFormat.trueColour = true;
	rfbScreen->serverFormat.bitsPerPixel = 32;
}


// Width of the display in pixels rather than points.
double displayPixelWidth( CGDirectDisplayID display )
{
	double pixelWidth = CGDisplayBounds( display ).size.width;

	if( CGDisplayModeRef mode = CGDisplayCopyDisplayMode( display ) )
	{
		const auto modePixelWidth = CGDisplayModeGetPixelWidth( mode );
		if( modePixelWidth > 0 )
		{
			pixelWidth = static_cast<double>( modePixelWidth );
		}
		CGDisplayModeRelease( mode );
	}

	return pixelWidth;
}


// Framebuffer pixels per point of the display - 2.0 on a Retina screen.
double displayScaleFactor( CGDirectDisplayID display )
{
	const auto pointWidth = CGDisplayBounds( display ).size.width;
	if( pointWidth <= 0 )
	{
		return 1.0;
	}

	return displayPixelWidth( display ) / pointWidth;
}


// The scale to capture at when none is configured. A Retina display has three
// to four times as many pixels as the Full HD screen of a typical Windows
// client, and every one of them has to be compared, encoded, sent and decoded
// again by the master - which is what made a Mac feel sluggish next to a
// Windows machine. Limiting the framebuffer to Full HD keeps text legible while
// putting a Mac on par with a Windows client.
double automaticCaptureScale( CGDirectDisplayID display )
{
	constexpr double MaxAutomaticLongEdge = 1920;

	const auto bounds = CGDisplayBounds( display );
	const auto scaleFactor = displayScaleFactor( display );
	const auto longEdge = std::max( bounds.size.width, bounds.size.height ) * scaleFactor;

	if( longEdge <= MaxAutomaticLongEdge )
	{
		return 1.0;
	}

	return MaxAutomaticLongEdge / longEdge;
}


// Build a LibVNCServer cursor from a snapshot of the system cursor. Ownership
// of everything allocated here passes to LibVNCServer through the cleanup
// flags; `source` is left out on purpose because LibVNCServer derives it
// itself (and frees it again) whenever a client asks for an X-style cursor.
rfbCursorPtr makeCursor( const MacVncCursorShape& shape )
{
	const auto pixelCount = static_cast<size_t>( shape.width ) * static_cast<size_t>( shape.height );

	auto* cursor = static_cast<rfbCursorPtr>( calloc( 1, sizeof( rfbCursor ) ) );
	auto* richSource = static_cast<unsigned char *>( malloc( pixelCount * 4 ) );
	auto* alphaSource = static_cast<unsigned char *>( malloc( pixelCount ) );

	if( cursor == nullptr || richSource == nullptr || alphaSource == nullptr )
	{
		free( cursor );
		free( richSource );
		free( alphaSource );
		return nullptr;
	}

	std::memcpy( richSource, shape.pixels.data(), pixelCount * 4 );
	std::memcpy( alphaSource, shape.alpha.data(), pixelCount );

	cursor->width = static_cast<unsigned short>( shape.width );
	cursor->height = static_cast<unsigned short>( shape.height );
	cursor->xhot = static_cast<unsigned short>( shape.hotspotX );
	cursor->yhot = static_cast<unsigned short>( shape.hotspotY );
	cursor->richSource = richSource;
	cursor->alphaSource = alphaSource;
	cursor->alphaPreMultiplied = TRUE;
	cursor->mask = reinterpret_cast<unsigned char *>(
		rfbMakeMaskFromAlphaSource( shape.width, shape.height, alphaSource ) );

	if( cursor->mask == nullptr )
	{
		free( cursor );
		free( richSource );
		free( alphaSource );
		return nullptr;
	}

	cursor->cleanup = TRUE;
	cursor->cleanupRichSource = TRUE;
	cursor->cleanupMask = TRUE;

	return cursor;
}

} // namespace


// ---- plugin --------------------------------------------------------------

MacVncServer::MacVncServer( QObject* parent ) :
	QObject( parent ),
	m_configuration( &VeyonCore::config() )
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

	// Without the Accessibility permission macOS silently drops every injected
	// mouse/keyboard event - the Master sees the screen but cannot control it.
	// Ask for it (shows the system dialog once) and say so in the log.
	{
		const void* keys[] = { kAXTrustedCheckOptionPrompt };
		const void* values[] = { kCFBooleanTrue };
		auto options = CFDictionaryCreate( kCFAllocatorDefault, keys, values, 1,
										   &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		const bool trusted = AXIsProcessTrustedWithOptions( options );
		CFRelease( options );
		fprintf( stderr, "[MacVncServer] Accessibility permission (remote input) = %s\n", trusted ? "granted" : "MISSING" );
		if( trusted == false )
		{
			vWarning() << "Accessibility permission missing - remote mouse/keyboard input is ignored by macOS."
					   << "Grant it in System Settings > Privacy & Security > Accessibility and restart the server.";
		}
	}

	MacVncScreen screen;

	if( initScreen( &screen ) == false ||
		initVncServer( serverPort, password, &screen ) == false )
	{
		return false;
	}

	// no client connected yet, so the capture may idle right away
	bool idle = false;

	while( rfbIsActive( screen.rfbScreen ) )
	{
		if( macScreenCaptureRunning() == false )
		{
			recoverCapture( &screen );
		}

		const bool hasClients = screen.rfbScreen->clientHead != nullptr;
		if( hasClients == idle )
		{
			idle = hasClients == false;
			macScreenCaptureSetIdle( idle );
		}

		if( hasClients == false )
		{
			// Nobody is watching this machine - which is the normal state for
			// most of the day. Diffing megabytes of pixels for no one is pure
			// waste, so idle instead: the capture keeps track of everything
			// that changes meanwhile and hands it over in one go as soon as a
			// client shows up.
			rfbProcessEvents( screen.rfbScreen, IdlePollIntervalMs * 1000 );
			continue;
		}

		MacScreenCaptureRegion changedRegion;

		// Frames are captured asynchronously on ScreenCaptureKit's own queue.
		// Waiting for the next one here - rather than polling on a fixed tick -
		// turns a frame into an RFB update the moment it arrives, and costs
		// nothing at all while the screen stays still.
		if( macScreenCaptureFrame( screen.captured, &changedRegion, FrameWaitMs ) )
		{
			updateChangedTiles( screen.rfbScreen,
								reinterpret_cast<uchar *>( screen.framebuffer.bits() ),
								static_cast<int>( screen.framebuffer.bytesPerLine() ),
								screen.captured, changedRegion );
		}

		updateCursor( &screen );

		rfbProcessEvents( screen.rfbScreen, ClientPollIntervalMs * 1000 );
	}

	rfbShutdownServer( screen.rfbScreen, true );
	rfbScreenCleanup( screen.rfbScreen );
	macVncCursorCleanup();
	macScreenCaptureCleanup();

	return true;
}



MacScreenCaptureOptions MacVncServer::prepareCapture( CGDirectDisplayID display )
{
	MacScreenCaptureOptions options;
	const auto captureScale = m_configuration.captureScale();
	options.scale = captureScale > 0 ? qBound( 10, captureScale, 100 ) / 100.0
									 : automaticCaptureScale( display );
	options.frameRate = m_configuration.captureFrameRate();

	// The cursor can only be kept out of the captured frames if we are able to
	// read its shape ourselves - otherwise clients would see no cursor at all.
	// Scale it the same way as the framebuffer so it keeps the size it has on
	// the local screen.
	m_remoteCursor = m_configuration.remoteCursor() &&
					 macVncCursorInit( options.scale * displayScaleFactor( display ) );
	options.captureCursor = m_remoteCursor == false;

	return options;
}



bool MacVncServer::initScreen( MacVncScreen* screen )
{
	screen->display = CGMainDisplayID();

	const auto options = prepareCapture( screen->display );

	vDebug() << "MacVncServer: mouse cursor is"
			 << ( m_remoteCursor ? "sent as a VNC cursor shape" : "captured as part of the screen" );

	int width = 0;
	int height = 0;

	if( macScreenCaptureInit( screen->display, options, &width, &height ) == false ||
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

	vDebug() << "MacVncServer: capturing at" << width << 'x' << height
			 << "scale" << options.scale;

	resizeFramebuffer( screen, width, height );

	return true;
}



void MacVncServer::resizeFramebuffer( MacVncScreen* screen, int width, int height )
{
	screen->framebuffer = QImage( width, height, QImage::Format_RGB32 );
	screen->framebuffer.fill( Qt::black );

	screen->captured = QImage( width, height, QImage::Format_RGB32 );
	screen->captured.fill( Qt::black );

	// map the RFB framebuffer (pixels) onto the display bounds (points)
	macVncInputInit( screen->display, CGDisplayBounds( screen->display ), width, height );
}



void MacVncServer::recoverCapture( MacVncScreen* screen )
{
	// Called whenever no stream is delivering frames: ScreenCaptureKit stops
	// the stream on its own when the display setup changes or the Screen
	// Recording permission is withdrawn, and the very first attempt may have
	// failed because the permission had not been granted yet. Without this the
	// clients would stare at a frozen screen until the server is restarted.
	if( m_captureRecoveryTimer.isValid() &&
		m_captureRecoveryTimer.elapsed() < m_captureRecoveryDelay )
	{
		return;
	}

	m_captureRecoveryTimer.restart();

	// Starting a stream means blocking this thread - and with it every
	// connected client - in ScreenCaptureKit's timeouts for several seconds.
	// The permission check is instant, so the common case of a permission that
	// was never granted never gets that far.
	if( macScreenCaptureAccessGranted() == false )
	{
		backOffCaptureRecovery();
		return;
	}

	// The display setup is a likely reason for the stream to have stopped, so
	// do not insist on the display that was the main one back then.
	screen->display = CGMainDisplayID();

	const auto options = prepareCapture( screen->display );

	if( m_remoteCursor == false )
	{
		// The cursor is part of the captured pixels again. Drop the shape the
		// clients were handed earlier, or they would keep drawing it frozen at
		// its last position on top of the captured one.
		rfbSetCursor( screen->rfbScreen, nullptr );
	}

	int width = 0;
	int height = 0;

	if( macScreenCaptureInit( screen->display, options, &width, &height ) == false ||
		width <= 0 || height <= 0 )
	{
		backOffCaptureRecovery();
		vWarning() << "MacVncServer: screen capture unavailable, retrying in"
				   << m_captureRecoveryDelay / 1000 << "s";
		return;
	}

	m_captureRecoveryDelay = CaptureRecoveryIntervalMs;

	vDebug() << "MacVncServer: screen capture running at" << width << 'x' << height;

	if( width != screen->framebuffer.width() || height != screen->framebuffer.height() )
	{
		// the display resolution changed - hand the clients a new framebuffer
		resizeFramebuffer( screen, width, height );
		rfbNewFramebuffer( screen->rfbScreen, reinterpret_cast<char *>( screen->framebuffer.bits() ),
						   width, height, 8, 3, 4 );
		applyServerFormat( screen->rfbScreen );
	}

	rfbMarkRectAsModified( screen->rfbScreen, 0, 0, screen->rfbScreen->width, screen->rfbScreen->height );
}



void MacVncServer::backOffCaptureRecovery()
{
	// retrying every couple of seconds forever would keep stalling the clients,
	// so slow down while the capture stays unavailable
	m_captureRecoveryDelay = std::min( m_captureRecoveryDelay * 2, MaxCaptureRecoveryIntervalMs );
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

	applyServerFormat( rfbScreen );

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



void MacVncServer::updateCursor( MacVncScreen* screen )
{
	if( m_remoteCursor == false )
	{
		return;
	}

	MacVncCursorShape shape;
	if( macVncCursorShapeChanged( &shape ) )
	{
		if( auto* cursor = makeCursor( shape ) )
		{
			vDebug() << "MacVncServer: new cursor shape" << shape.width << 'x' << shape.height
					 << "hotspot" << shape.hotspotX << shape.hotspotY;

			// also frees the cursor set before
			rfbSetCursor( screen->rfbScreen, cursor );
		}
	}

	int x = 0;
	int y = 0;

	if( macVncInputMapToFramebuffer( macVncCursorPosition(), &x, &y ) &&
		( x != screen->rfbScreen->cursorX || y != screen->rfbScreen->cursorY ) )
	{
		screen->rfbScreen->cursorX = x;
		screen->rfbScreen->cursorY = y;

		// Clients drawing the cursor themselves have to be told that it moved.
		// For the others LibVNCServer compares against the position it last
		// sent them and redraws on its own.
		auto* iterator = rfbGetClientIterator( screen->rfbScreen );
		while( auto* client = rfbClientIteratorNext( iterator ) )
		{
			if( client->enableCursorPosUpdates )
			{
				client->cursorWasMoved = TRUE;
			}
		}
		rfbReleaseClientIterator( iterator );
	}
}



IMPLEMENT_CONFIG_PROXY(MacVncConfiguration)
