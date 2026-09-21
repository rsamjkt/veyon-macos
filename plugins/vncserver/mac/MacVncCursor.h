/*
 * MacVncCursor.h - system cursor tracking for the macOS VNC server
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

#include <cstdint>
#include <vector>

// A snapshot of the system cursor, ready to be handed to LibVNCServer.
struct MacVncCursorShape
{
	int width = 0;
	int height = 0;
	int hotspotX = 0;
	int hotspotY = 0;

	// width*height pixels, BGRA with premultiplied alpha - the same byte order
	// as the framebuffer, so this can be handed to LibVNCServer as richSource
	std::vector<uint8_t> pixels;

	// width*height alpha values, extracted from `pixels` for LibVNCServer
	std::vector<uint8_t> alpha;

	bool isEmpty() const { return width <= 0 || height <= 0; }
};

// Start tracking the system cursor, rendering it at the given scale factor
// (framebuffer pixels per point, i.e. 2.0 on a Retina display).
//
// Reading the cursor image requires AppKit and therefore the main thread, so a
// timer on the main queue keeps a snapshot up to date which the VNC thread can
// pick up at any time without blocking.
//
// Returns false when the cursor cannot be read at all - the caller should then
// fall back to letting ScreenCaptureKit draw the cursor into the frames.
bool macVncCursorInit( double scale );

// Copy the current cursor shape. Returns false if it has not changed since the
// previous call, in which case `shape` is left untouched.
bool macVncCursorShapeChanged( MacVncCursorShape* shape );

// Current pointer position in global display coordinates (points).
CGPoint macVncCursorPosition();

// Stop tracking the system cursor.
void macVncCursorCleanup();
