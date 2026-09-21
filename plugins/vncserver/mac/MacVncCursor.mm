/*
 * MacVncCursor.mm - system cursor tracking for the macOS VNC server
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

#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>

#include "MacVncCursor.h"


namespace {

// how often the system cursor is re-read; cursors change only when the pointer
// moves over a different kind of element, so this is far from a hot path
constexpr int64_t RefreshIntervalMs = 100;

// cursors bigger than this are almost certainly a misread - do not let them
// blow up the VNC cursor updates
constexpr int MaxCursorSize = 256;

std::mutex g_mutex;
MacVncCursorShape g_shape;
uint64_t g_serial = 0;
uint64_t g_consumed = 0;
uint64_t g_shapeHash = 0;

// written once before the refresh timer starts and only read afterwards
double g_scale = 1.0;

dispatch_source_t g_timer = nullptr;


uint64_t hashShape( const MacVncCursorShape& shape )
{
	// FNV-1a over the pixels plus the metrics - just enough to notice that the
	// cursor turned into a different one
	uint64_t hash = 0xcbf29ce484222325ULL;

	const auto feed = [&hash]( uint64_t value ) {
		hash = ( hash ^ value ) * 0x100000001b3ULL;
	};

	feed( static_cast<uint64_t>( shape.width ) );
	feed( static_cast<uint64_t>( shape.height ) );
	feed( static_cast<uint64_t>( shape.hotspotX ) );
	feed( static_cast<uint64_t>( shape.hotspotY ) );

	for( const auto byte : shape.pixels )
	{
		feed( byte );
	}

	return hash;
}


// Render the current system cursor into a BGRA bitmap. Must run on the main
// thread - NSCursor is AppKit.
bool renderSystemCursor( MacVncCursorShape* shape )
{
	NSCursor* cursor = [NSCursor currentSystemCursor];
	NSImage* image = cursor.image;

	if( cursor == nil || image == nil )
	{
		return false;
	}

	const NSSize size = image.size;
	if( size.width <= 0 || size.height <= 0 )
	{
		return false;
	}

	const int width = std::clamp( static_cast<int>( std::lround( size.width * g_scale ) ), 1, MaxCursorSize );
	const int height = std::clamp( static_cast<int>( std::lround( size.height * g_scale ) ), 1, MaxCursorSize );

	shape->width = width;
	shape->height = height;
	shape->pixels.assign( static_cast<size_t>( width ) * static_cast<size_t>( height ) * 4, 0 );

	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
	if( colorSpace == nullptr )
	{
		return false;
	}

	// premultiplied BGRA, matching both the framebuffer layout and what
	// LibVNCServer expects from a richSource with alphaPreMultiplied set
	CGContextRef context = CGBitmapContextCreate( shape->pixels.data(), static_cast<size_t>( width ),
												  static_cast<size_t>( height ), 8,
												  static_cast<size_t>( width ) * 4, colorSpace,
												  static_cast<uint32_t>( kCGImageAlphaPremultipliedFirst ) |
												  static_cast<uint32_t>( kCGBitmapByteOrder32Little ) );
	CGColorSpaceRelease( colorSpace );

	if( context == nullptr )
	{
		return false;
	}

	NSRect imageRect = NSMakeRect( 0, 0, size.width, size.height );
	CGImageRef cgImage = [image CGImageForProposedRect:&imageRect context:nil hints:nil];

	if( cgImage != nullptr )
	{
		// no flipping needed: a bitmap context stores its first row at the top
		// of the buffer, which is exactly the row order RFB expects
		CGContextDrawImage( context, CGRectMake( 0, 0, width, height ), cgImage );
	}

	CGContextRelease( context );

	if( cgImage == nullptr )
	{
		return false;
	}

	const auto pixelCount = static_cast<size_t>( width ) * static_cast<size_t>( height );
	shape->alpha.resize( pixelCount );
	for( size_t i = 0; i < pixelCount; ++i )
	{
		shape->alpha[i] = shape->pixels[i*4 + 3];
	}

	const NSPoint hotSpot = cursor.hotSpot;
	shape->hotspotX = std::clamp( static_cast<int>( std::lround( hotSpot.x * g_scale ) ), 0, width - 1 );
	shape->hotspotY = std::clamp( static_cast<int>( std::lround( hotSpot.y * g_scale ) ), 0, height - 1 );

	return true;
}


void refreshCursorShape()
{
	@autoreleasepool {
		MacVncCursorShape shape;
		if( renderSystemCursor( &shape ) == false )
		{
			return;
		}

		const auto hash = hashShape( shape );

		const std::lock_guard<std::mutex> lock( g_mutex );
		if( hash != g_shapeHash )
		{
			g_shapeHash = hash;
			g_shape = std::move( shape );
			++g_serial;
		}
	}
}

} // namespace


bool macVncCursorInit( double scale )
{
	macVncCursorCleanup();

	g_scale = scale > 0 ? scale : 1.0;

	// Probe once before committing to the cursor-through-RFB path: if AppKit
	// hands out no cursor (which it may in a session without a window server
	// connection) the caller has to keep the cursor in the captured frames.
	__block bool available = false;
	dispatch_semaphore_t probe = dispatch_semaphore_create( 0 );

	dispatch_async( dispatch_get_main_queue(), ^{
		refreshCursorShape();
		{
			const std::lock_guard<std::mutex> lock( g_mutex );
			available = g_shape.isEmpty() == false;
		}
		dispatch_semaphore_signal( probe );
	} );

	if( dispatch_semaphore_wait( probe, dispatch_time( DISPATCH_TIME_NOW, 2LL * NSEC_PER_SEC ) ) != 0 ||
		available == false )
	{
		macVncCursorCleanup();
		return false;
	}

	g_timer = dispatch_source_create( DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue() );
	if( g_timer == nullptr )
	{
		macVncCursorCleanup();
		return false;
	}

	dispatch_source_set_timer( g_timer, dispatch_time( DISPATCH_TIME_NOW, RefreshIntervalMs * NSEC_PER_MSEC ),
							   static_cast<uint64_t>( RefreshIntervalMs ) * NSEC_PER_MSEC,
							   10 * NSEC_PER_MSEC );
	dispatch_source_set_event_handler( g_timer, ^{ refreshCursorShape(); } );
	dispatch_resume( g_timer );

	return true;
}



bool macVncCursorShapeChanged( MacVncCursorShape* shape )
{
	const std::lock_guard<std::mutex> lock( g_mutex );

	if( g_serial == g_consumed || g_shape.isEmpty() )
	{
		return false;
	}

	g_consumed = g_serial;
	*shape = g_shape;

	return true;
}



CGPoint macVncCursorPosition()
{
	CGPoint position = CGPointZero;

	// CGEvent is safe to use from any thread, unlike NSEvent.mouseLocation
	CGEventRef event = CGEventCreate( nullptr );
	if( event != nullptr )
	{
		position = CGEventGetLocation( event );
		CFRelease( event );
	}

	return position;
}



void macVncCursorCleanup()
{
	if( g_timer != nullptr )
	{
		dispatch_source_cancel( g_timer );
		g_timer = nullptr;
	}

	const std::lock_guard<std::mutex> lock( g_mutex );
	g_shape = {};
	g_serial = 0;
	g_consumed = 0;
	g_shapeHash = 0;
}
