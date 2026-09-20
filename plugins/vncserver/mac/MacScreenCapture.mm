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
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <dispatch/dispatch.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#include <QImage>

#include "MacScreenCapture.h"

namespace {

constexpr int DefaultFrameRate = 30;
constexpr int MaxFrameRate = 60;

// how often the whole frame is copied and diffed regardless of what
// ScreenCaptureKit reported as dirty
constexpr int FullRefreshIntervalSeconds = 2;

std::mutex g_mutex;
std::condition_variable g_frameAvailable;

// latest captured frame, BGRA, g_width*4 bytes per line - matches
// QImage::Format_RGB32 byte-for-byte on little-endian
std::vector<uint8_t> g_frame;
int g_width = 0;
int g_height = 0;

// bumped for every frame the stream delivers; the consumer tracks how far it
// has read so that an unchanged screen costs nothing at all
uint64_t g_serial = 0;
uint64_t g_consumed = 0;

// union of the rectangles changed since the consumer last read a frame,
// as a half-open range [x0,x1) x [y0,y1)
int g_dirtyX0 = 0;
int g_dirtyY0 = 0;
int g_dirtyX1 = 0;
int g_dirtyY1 = 0;

// forces the next frame to be copied and diffed in full
bool g_fullRefreshDue = true;
int g_framesSinceFullRefresh = 0;
int g_fullRefreshInterval = 0;

bool g_ignoreDirtyRects = false;
bool g_streamStopped = false;


void markDirty( int x0, int y0, int x1, int y1 )
{
	if( g_dirtyX1 <= g_dirtyX0 || g_dirtyY1 <= g_dirtyY0 )
	{
		g_dirtyX0 = x0;
		g_dirtyY0 = y0;
		g_dirtyX1 = x1;
		g_dirtyY1 = y1;
		return;
	}

	g_dirtyX0 = std::min( g_dirtyX0, x0 );
	g_dirtyY0 = std::min( g_dirtyY0, y0 );
	g_dirtyX1 = std::max( g_dirtyX1, x1 );
	g_dirtyY1 = std::max( g_dirtyY1, y1 );
}


void storeFrame( CVPixelBufferRef pixelBuffer, CFArrayRef dirtyRects )
{
	if( CVPixelBufferLockBaseAddress( pixelBuffer, kCVPixelBufferLock_ReadOnly ) != kCVReturnSuccess )
	{
		return;
	}

	const auto* source = static_cast<const uint8_t *>( CVPixelBufferGetBaseAddress( pixelBuffer ) );
	if( source != nullptr )
	{
		const size_t sourceStride = CVPixelBufferGetBytesPerRow( pixelBuffer );
		const int sourceWidth = static_cast<int>( CVPixelBufferGetWidth( pixelBuffer ) );
		const int sourceHeight = static_cast<int>( CVPixelBufferGetHeight( pixelBuffer ) );

		const std::lock_guard<std::mutex> lock( g_mutex );

		const int width = std::min( sourceWidth, g_width );
		const int height = std::min( sourceHeight, g_height );

		// Work out which part of the frame ScreenCaptureKit says has changed.
		// Anything we cannot make sense of falls back to the whole frame, and a
		// full refresh is forced periodically anyway.
		int x0 = 0;
		int y0 = 0;
		int x1 = width;
		int y1 = height;

		const auto rectCount = dirtyRects != nullptr ? CFArrayGetCount( dirtyRects ) : 0;

		if( g_fullRefreshDue == false && g_ignoreDirtyRects == false && rectCount > 0 )
		{
			bool valid = true;
			int left = width;
			int top = height;
			int right = 0;
			int bottom = 0;

			for( CFIndex i = 0; i < rectCount; ++i )
			{
				CGRect rect = CGRectZero;
				auto dict = static_cast<CFDictionaryRef>( CFArrayGetValueAtIndex( dirtyRects, i ) );
				if( dict == nullptr || CGRectMakeWithDictionaryRepresentation( dict, &rect ) == false )
				{
					valid = false;
					break;
				}

				left = std::min( left, static_cast<int>( std::floor( CGRectGetMinX( rect ) ) ) );
				top = std::min( top, static_cast<int>( std::floor( CGRectGetMinY( rect ) ) ) );
				right = std::max( right, static_cast<int>( std::ceil( CGRectGetMaxX( rect ) ) ) );
				bottom = std::max( bottom, static_cast<int>( std::ceil( CGRectGetMaxY( rect ) ) ) );
			}

			if( valid )
			{
				x0 = std::clamp( left, 0, width );
				y0 = std::clamp( top, 0, height );
				x1 = std::clamp( right, x0, width );
				y1 = std::clamp( bottom, y0, height );
			}
		}

		if( x1 > x0 && y1 > y0 )
		{
			const size_t stride = static_cast<size_t>( g_width ) * 4;
			const size_t offset = static_cast<size_t>( x0 ) * 4;
			const size_t rowBytes = static_cast<size_t>( x1 - x0 ) * 4;

			for( int y = y0; y < y1; ++y )
			{
				std::memcpy( g_frame.data() + static_cast<size_t>( y )*stride + offset,
							 source + static_cast<size_t>( y )*sourceStride + offset, rowBytes );
			}

			markDirty( x0, y0, x1, y1 );
			++g_serial;
		}

		g_fullRefreshDue = false;
		if( ++g_framesSinceFullRefresh >= g_fullRefreshInterval )
		{
			g_framesSinceFullRefresh = 0;
			g_fullRefreshDue = true;
		}
	}

	CVPixelBufferUnlockBaseAddress( pixelBuffer, kCVPixelBufferLock_ReadOnly );

	g_frameAvailable.notify_all();
}

} // namespace


@interface VeyonScreenCaptureOutput : NSObject <SCStreamOutput, SCStreamDelegate>
@end

@implementation VeyonScreenCaptureOutput

- (void)stream:(SCStream *)stream
	didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
	ofType:(SCStreamOutputType)type
{
	Q_UNUSED(stream)

	if( type != SCStreamOutputTypeScreen || CMSampleBufferIsValid( sampleBuffer ) == NO )
	{
		return;
	}

	// ScreenCaptureKit keeps delivering buffers while the screen is idle; those
	// carry no pixels and must not be treated as a new frame
	CFArrayRef dirtyRects = nullptr;

	NSArray* attachments = (__bridge NSArray *)CMSampleBufferGetSampleAttachmentsArray( sampleBuffer, NO );
	if( attachments.count > 0 )
	{
		NSDictionary* frameInfo = attachments.firstObject;

		NSNumber* status = frameInfo[SCStreamFrameInfoStatus];
		if( status != nil && status.intValue != SCFrameStatusComplete )
		{
			return;
		}

		dirtyRects = (__bridge CFArrayRef)frameInfo[SCStreamFrameInfoDirtyRects];
	}

	CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer( sampleBuffer );
	if( pixelBuffer != nullptr )
	{
		storeFrame( pixelBuffer, dirtyRects );
	}
}


- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
	Q_UNUSED(stream)

	fprintf( stderr, "[MacVncServer] capture stream stopped: %s\n",
			 error != nil ? error.localizedDescription.UTF8String : "unknown error" );

	{
		const std::lock_guard<std::mutex> lock( g_mutex );
		g_streamStopped = true;
	}

	g_frameAvailable.notify_all();
}

@end


namespace {

SCStream* g_stream = nil;
VeyonScreenCaptureOutput* g_output = nil;
dispatch_queue_t g_queue = nullptr;


double captureScale()
{
	const char* value = std::getenv( "VEYON_MAC_CAPTURE_SCALE" ); // Flawfinder: ignore
	if( value == nullptr )
	{
		return 1.0;
	}

	const double scale = std::atof( value );
	return scale >= 0.1 && scale <= 1.0 ? scale : 1.0;
}


int captureFrameRate()
{
	const char* value = std::getenv( "VEYON_MAC_CAPTURE_FPS" ); // Flawfinder: ignore
	if( value == nullptr )
	{
		return DefaultFrameRate;
	}

	const int fps = std::atoi( value );
	return fps >= 1 && fps <= MaxFrameRate ? fps : DefaultFrameRate;
}


bool ignoreDirtyRects()
{
	const char* value = std::getenv( "VEYON_MAC_CAPTURE_FULL_DIFF" ); // Flawfinder: ignore
	return value != nullptr && std::atoi( value ) != 0;
}


SCDisplay* findDisplay( CGDirectDisplayID display )
{
	__block SCDisplay* targetDisplay = nil;

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
		}
		dispatch_semaphore_signal( sem );
	}];

	// wait up to 5 seconds for the (asynchronous) permission/content query
	if( dispatch_semaphore_wait( sem, dispatch_time( DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC ) ) != 0 )
	{
		return nil;
	}

	return targetDisplay;
}


// SCDisplay reports its size in points. Ask CoreGraphics for the number of
// pixels the display actually renders, so that a Retina screen is not captured
// at half resolution.
void pixelDimensions( SCDisplay* display, int* width, int* height )
{
	*width = static_cast<int>( display.width );
	*height = static_cast<int>( display.height );

	CGDisplayModeRef mode = CGDisplayCopyDisplayMode( display.displayID );
	if( mode != nullptr )
	{
		const auto pixelWidth = static_cast<int>( CGDisplayModeGetPixelWidth( mode ) );
		const auto pixelHeight = static_cast<int>( CGDisplayModeGetPixelHeight( mode ) );
		if( pixelWidth > 0 && pixelHeight > 0 )
		{
			*width = pixelWidth;
			*height = pixelHeight;
		}
		CGDisplayModeRelease( mode );
	}
}

} // namespace


bool macScreenCaptureInit( CGDirectDisplayID display, int* outWidth, int* outHeight )
{
	macScreenCaptureCleanup();

	SCDisplay* targetDisplay = findDisplay( display );
	if( targetDisplay == nil )
	{
		return false;
	}

	int width = 0;
	int height = 0;
	pixelDimensions( targetDisplay, &width, &height );

	const double scale = captureScale();
	if( scale < 1.0 )
	{
		// keep both dimensions even so that the 4-byte rows stay aligned
		width = std::max( 2, static_cast<int>( width * scale ) & ~1 );
		height = std::max( 2, static_cast<int>( height * scale ) & ~1 );
	}

	if( width <= 0 || height <= 0 )
	{
		return false;
	}

	const int frameRate = captureFrameRate();

	SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:targetDisplay excludingWindows:@[]];

	SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
	config.width = static_cast<size_t>( width );
	config.height = static_cast<size_t>( height );
	config.pixelFormat = kCVPixelFormatType_32BGRA;
	config.colorSpaceName = kCGColorSpaceSRGB;
	config.showsCursor = YES;
	config.queueDepth = 5;
	config.minimumFrameInterval = CMTimeMake( 1, frameRate );

	g_output = [[VeyonScreenCaptureOutput alloc] init];
	g_queue = dispatch_queue_create( "io.veyon.server.screencapture", DISPATCH_QUEUE_SERIAL );

	{
		const std::lock_guard<std::mutex> lock( g_mutex );
		g_width = width;
		g_height = height;
		g_frame.assign( static_cast<size_t>( width ) * static_cast<size_t>( height ) * 4, 0 );
		g_serial = 0;
		g_consumed = 0;
		g_dirtyX0 = g_dirtyY0 = g_dirtyX1 = g_dirtyY1 = 0;
		g_fullRefreshDue = true;
		g_framesSinceFullRefresh = 0;
		g_fullRefreshInterval = frameRate * FullRefreshIntervalSeconds;
		g_ignoreDirtyRects = ignoreDirtyRects();
		g_streamStopped = false;
	}

	NSError* error = nil;
	g_stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:g_output];
	if( [g_stream addStreamOutput:g_output
							 type:SCStreamOutputTypeScreen
			   sampleHandlerQueue:g_queue
							error:&error] == NO )
	{
		fprintf( stderr, "[MacVncServer] could not add capture output: %s\n",
				 error != nil ? error.localizedDescription.UTF8String : "unknown error" );
		macScreenCaptureCleanup();
		return false;
	}

	__block bool started = false;
	dispatch_semaphore_t sem = dispatch_semaphore_create( 0 );
	[g_stream startCaptureWithCompletionHandler:^( NSError* startError ) {
		if( startError != nil )
		{
			fprintf( stderr, "[MacVncServer] could not start capture stream: %s\n",
					 startError.localizedDescription.UTF8String );
		}
		else
		{
			started = true;
		}
		dispatch_semaphore_signal( sem );
	}];

	if( dispatch_semaphore_wait( sem, dispatch_time( DISPATCH_TIME_NOW, 5LL * NSEC_PER_SEC ) ) != 0 ||
		started == false )
	{
		macScreenCaptureCleanup();
		return false;
	}

	fprintf( stderr, "[MacVncServer] screen capture ready: %dx%d @ %d fps (display is %.0fx%.0f pt), "
					 "Screen Recording permission = %s\n",
			 width, height, frameRate,
			 static_cast<double>( targetDisplay.width ), static_cast<double>( targetDisplay.height ),
			 CGPreflightScreenCaptureAccess()
				 ? "granted"
				 : "DENIED - grant it in System Settings > Privacy & Security > Screen Recording and restart the server" );

	if( outWidth != nullptr ) { *outWidth = width; }
	if( outHeight != nullptr ) { *outHeight = height; }

	return true;
}



bool macScreenCaptureFrame( QImage& target, MacScreenCaptureRegion* changedRegion, int timeoutMs )
{
	std::unique_lock<std::mutex> lock( g_mutex );

	if( g_frame.empty() )
	{
		return false;
	}

	if( g_serial == g_consumed )
	{
		if( timeoutMs <= 0 )
		{
			return false;
		}

		g_frameAvailable.wait_for( lock, std::chrono::milliseconds( timeoutMs ),
								   []{ return g_serial != g_consumed || g_streamStopped; } );

		if( g_serial == g_consumed )
		{
			return false;
		}
	}

	const int x0 = std::clamp( g_dirtyX0, 0, std::min( g_width, target.width() ) );
	const int y0 = std::clamp( g_dirtyY0, 0, std::min( g_height, target.height() ) );
	const int x1 = std::clamp( g_dirtyX1, x0, std::min( g_width, target.width() ) );
	const int y1 = std::clamp( g_dirtyY1, y0, std::min( g_height, target.height() ) );

	if( x1 <= x0 || y1 <= y0 )
	{
		g_consumed = g_serial;
		return false;
	}

	const size_t stride = static_cast<size_t>( g_width ) * 4;
	const size_t offset = static_cast<size_t>( x0 ) * 4;
	const size_t rowBytes = static_cast<size_t>( x1 - x0 ) * 4;

	for( int y = y0; y < y1; ++y )
	{
		std::memcpy( target.scanLine( y ) + offset,
					 g_frame.data() + static_cast<size_t>( y )*stride + offset, rowBytes );
	}

	if( changedRegion != nullptr )
	{
		changedRegion->x = x0;
		changedRegion->y = y0;
		changedRegion->width = x1 - x0;
		changedRegion->height = y1 - y0;
	}

	g_dirtyX0 = g_dirtyY0 = g_dirtyX1 = g_dirtyY1 = 0;
	g_consumed = g_serial;

	return true;
}



void macScreenCaptureCleanup()
{
	if( g_stream != nil )
	{
		dispatch_semaphore_t sem = dispatch_semaphore_create( 0 );
		[g_stream stopCaptureWithCompletionHandler:^( NSError* ) {
			dispatch_semaphore_signal( sem );
		}];
		dispatch_semaphore_wait( sem, dispatch_time( DISPATCH_TIME_NOW, 3LL * NSEC_PER_SEC ) );
		g_stream = nil;
	}

	g_output = nil;
	g_queue = nullptr;

	const std::lock_guard<std::mutex> lock( g_mutex );
	g_frame.clear();
	g_frame.shrink_to_fit();
	g_width = 0;
	g_height = 0;
	g_serial = 0;
	g_consumed = 0;
	g_dirtyX0 = g_dirtyY0 = g_dirtyX1 = g_dirtyY1 = 0;
	g_fullRefreshDue = true;
	g_framesSinceFullRefresh = 0;
	g_streamStopped = false;
}
