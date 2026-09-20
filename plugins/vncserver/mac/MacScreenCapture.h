/*
 * MacScreenCapture.h - ScreenCaptureKit-based screen capture for the VNC server
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

#include <CoreGraphics/CoreGraphics.h>

class QImage;

// The rectangle of the framebuffer that changed since the previous frame.
struct MacScreenCaptureRegion
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;

	bool isEmpty() const { return width <= 0 || height <= 0; }
};

// Start a continuous ScreenCaptureKit stream for the given display and report
// the capture dimensions via outWidth/outHeight.
//
// The dimensions are the display's native PIXEL dimensions, not its size in
// points: SCDisplay.width/height are points, so capturing at those on a Retina
// display yields a half-resolution (visibly blurry) framebuffer.
//
// Two environment variables tune the stream:
//   VEYON_MAC_CAPTURE_SCALE  0.1 .. 1.0, downscales the capture (default 1.0 =
//                            native pixels). Lower it to trade sharpness for
//                            bandwidth on slow networks.
//   VEYON_MAC_CAPTURE_FPS    1 .. 60, maximum frame rate (default 30).
//   VEYON_MAC_CAPTURE_FULL_DIFF  set to 1 to ignore the dirty rectangles that
//                            ScreenCaptureKit reports and always treat the
//                            whole screen as changed (troubleshooting only).
//
// Returns false if ScreenCaptureKit is unavailable or the Screen Recording
// permission has not been granted.
bool macScreenCaptureInit( CGDirectDisplayID display, int* outWidth, int* outHeight );

// Copy the most recent frame into the given QImage (must be Format_RGB32 and
// sized to the dimensions returned by macScreenCaptureInit).
//
// Frames arrive asynchronously on the stream's own queue, so this never waits
// for the capture hardware. It returns false straight away when no new frame
// has arrived since the last call - waiting at most timeoutMs for one if
// timeoutMs is greater than zero.
//
// Only the region that changed is written, so `target` must be the same image
// across calls: it carries the previous frame's pixels. That region is
// reported through changedRegion, letting the caller restrict its own work to
// it as well. Every couple of seconds the full frame is refreshed regardless,
// bounding how long a missed dirty rectangle could go unnoticed.
bool macScreenCaptureFrame( QImage& target, MacScreenCaptureRegion* changedRegion = nullptr,
							int timeoutMs = 0 );

// Release ScreenCaptureKit resources.
void macScreenCaptureCleanup();
