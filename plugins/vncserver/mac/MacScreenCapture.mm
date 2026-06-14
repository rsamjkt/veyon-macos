/*
 * MacScreenCapture.mm - ScreenCaptureKit-based screen capture for the VNC server
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

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <dispatch/dispatch.h>

#include <cstdio>

#include <QImage>

#include "MacScreenCapture.h"

namespace {

SCContentFilter* g_filter = nil;
int g_width = 0;
int g_height = 0;

} // namespace


bool macScreenCaptureInit( CGDirectDisplayID display, int* outWidth, int* outHeight )
{
	__block SCDisplay* targetDisplay = nil;
	__block bool gotContent = false;

	dispatch_semaphore_t sem = dispatch_semaphore_create( 0 );
	[SCShareableContent getShareableContentWithCompletionHandler:^( SCShareableContent* content, NSError* error ) {
		if( content != nil && error == nil )
		{
			for( SCDisplay* d in content.displays )
			{
				if( d.displayID == display )
				{
					targetDisplay = d;
					break;
				}
			}
			if( targetDisplay == nil && content.displays.count > 0 )
			{
				targetDisplay = content.displays.firstObject;
			}
			gotContent = true;
		}
		dispatch_semaphore_signal( sem );
	}];

	// wait up to 5 seconds for the (asynchronous) permission/content query
	if( dispatch_semaphore_wait( sem, dispatch_time( DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC ) ) != 0 )
	{
		return false;
	}

	if( gotContent == false || targetDisplay == nil )
	{
		return false;
	}

	g_filter = [[SCContentFilter alloc] initWithDisplay:targetDisplay excludingWindows:@[]];
	g_width = (int)targetDisplay.width;
	g_height = (int)targetDisplay.height;

	fprintf( stderr, "[MacVncServer] screen capture ready: %dx%d, Screen Recording permission = %s\n",
			 g_width, g_height,
			 CGPreflightScreenCaptureAccess()
				 ? "granted"
				 : "DENIED - grant it in System Settings > Privacy & Security > Screen Recording and restart the server" );

	if( outWidth != nullptr ) { *outWidth = g_width; }
	if( outHeight != nullptr ) { *outHeight = g_height; }

	return g_width > 0 && g_height > 0;
}



bool macScreenCaptureFrame( QImage& target )
{
	if( g_filter == nil )
	{
		return false;
	}

	SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
	config.width = (size_t)target.width();
	config.height = (size_t)target.height();
	config.pixelFormat = kCVPixelFormatType_32BGRA;
	config.showsCursor = YES;

	__block CGImageRef captured = nullptr;
	dispatch_semaphore_t sem = dispatch_semaphore_create( 0 );

	[SCScreenshotManager captureImageWithFilter:g_filter
								  configuration:config
							  completionHandler:^( CGImageRef image, NSError* error ) {
		if( image != nullptr && error == nil )
		{
			captured = CGImageRetain( image );
		}
		dispatch_semaphore_signal( sem );
	}];

	if( dispatch_semaphore_wait( sem, dispatch_time( DISPATCH_TIME_NOW, 2LL * NSEC_PER_SEC ) ) != 0 )
	{
		return false;
	}

	if( captured == nullptr )
	{
		return false;
	}

	bool ok = false;
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGContextRef ctx = CGBitmapContextCreate( target.bits(),
											  (size_t)target.width(),
											  (size_t)target.height(),
											  8,
											  (size_t)target.bytesPerLine(),
											  colorSpace,
											  kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little );
	if( ctx != nullptr )
	{
		CGContextDrawImage( ctx, CGRectMake( 0, 0, target.width(), target.height() ), captured );
		CGContextRelease( ctx );
		ok = true;
	}

	CGColorSpaceRelease( colorSpace );
	CGImageRelease( captured );

	return ok;
}



void macScreenCaptureCleanup()
{
	g_filter = nil;
	g_width = 0;
	g_height = 0;
}
