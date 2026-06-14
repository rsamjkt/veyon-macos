/*
 * MacVideoWriter.mm - H.264 video writer (AVFoundation) for screen recording
 *
 * Copyright (c) 2026 Arunika / AruniControl
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

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <QImage>

#include "MacVideoWriter.h"


struct MacVideoWriter::Private
{
	AVAssetWriter* writer = nil;
	AVAssetWriterInput* input = nil;
	AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;
	int fps = 10;
	long long frameIndex = 0;
	QSize size;
	bool open = false;
};


MacVideoWriter::MacVideoWriter() :
	d( new Private )
{
}



MacVideoWriter::~MacVideoWriter()
{
	close();
	delete d;
}



bool MacVideoWriter::isOpen() const
{
	return d->open;
}



bool MacVideoWriter::open( const QString& filePath, const QSize& size, int fps )
{
	const int w = size.width() & ~1;   // H.264 needs even dimensions
	const int h = size.height() & ~1;
	if( w < 2 || h < 2 )
	{
		return false;
	}

	NSURL* url = [NSURL fileURLWithPath:filePath.toNSString()];
	[[NSFileManager defaultManager] removeItemAtURL:url error:nil];

	NSError* error = nil;
	d->writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeQuickTimeMovie error:&error];
	if( d->writer == nil )
	{
		return false;
	}

	NSDictionary* videoSettings = @{
		AVVideoCodecKey: AVVideoCodecTypeH264,
		AVVideoWidthKey: @(w),
		AVVideoHeightKey: @(h)
	};
	d->input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
												  outputSettings:videoSettings];
	d->input.expectsMediaDataInRealTime = YES;

	NSDictionary* bufferAttributes = @{
		(NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
		(NSString*)kCVPixelBufferWidthKey: @(w),
		(NSString*)kCVPixelBufferHeightKey: @(h)
	};
	d->adaptor = [AVAssetWriterInputPixelBufferAdaptor
				  assetWriterInputPixelBufferAdaptorWithAssetWriterInput:d->input
				  sourcePixelBufferAttributes:bufferAttributes];

	if( [d->writer canAddInput:d->input] == NO )
	{
		d->writer = nil; d->input = nil; d->adaptor = nil;
		return false;
	}
	[d->writer addInput:d->input];

	if( [d->writer startWriting] == NO )
	{
		d->writer = nil; d->input = nil; d->adaptor = nil;
		return false;
	}
	[d->writer startSessionAtSourceTime:kCMTimeZero];

	d->fps = fps > 0 ? fps : 10;
	d->frameIndex = 0;
	d->size = QSize( w, h );
	d->open = true;
	return true;
}



void MacVideoWriter::writeFrame( const QImage& image )
{
	if( d->open == false || d->input == nil || d->input.readyForMoreMediaData == NO )
	{
		return;
	}

	QImage frame = image.convertToFormat( QImage::Format_ARGB32 ); // -> BGRA bytes
	if( frame.size() != d->size )
	{
		frame = frame.scaled( d->size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
	}

	CVPixelBufferRef pixelBuffer = nullptr;
	CVPixelBufferPoolRef pool = d->adaptor.pixelBufferPool;
	if( pool != nullptr )
	{
		CVPixelBufferPoolCreatePixelBuffer( nullptr, pool, &pixelBuffer );
	}
	if( pixelBuffer == nullptr )
	{
		return;
	}

	CVPixelBufferLockBaseAddress( pixelBuffer, 0 );
	auto* dst = static_cast<uint8_t *>( CVPixelBufferGetBaseAddress( pixelBuffer ) );
	const size_t dstStride = CVPixelBufferGetBytesPerRow( pixelBuffer );
	const int rowBytes = qMin( static_cast<int>( dstStride ), frame.bytesPerLine() );
	for( int y = 0; y < d->size.height(); ++y )
	{
		memcpy( dst + y * dstStride, frame.constScanLine( y ), rowBytes );
	}
	CVPixelBufferUnlockBaseAddress( pixelBuffer, 0 );

	const CMTime presentationTime = CMTimeMake( d->frameIndex, d->fps );
	if( [d->adaptor appendPixelBuffer:pixelBuffer withPresentationTime:presentationTime] )
	{
		d->frameIndex++;
	}
	CVPixelBufferRelease( pixelBuffer );
}



void MacVideoWriter::close()
{
	if( d->writer == nil )
	{
		d->open = false;
		return;
	}

	[d->input markAsFinished];

	dispatch_semaphore_t sem = dispatch_semaphore_create( 0 );
	[d->writer finishWritingWithCompletionHandler:^{
		dispatch_semaphore_signal( sem );
	}];
	dispatch_semaphore_wait( sem, dispatch_time( DISPATCH_TIME_NOW, 15LL * NSEC_PER_SEC ) );

	d->writer = nil;
	d->input = nil;
	d->adaptor = nil;
	d->open = false;
}
