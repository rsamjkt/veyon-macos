/*
 * MacVncInput.h - remote input injection for the macOS VNC server
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

// Initialise the input mapping. The framebuffer (RFB) coordinate space is
// fbWidth x fbHeight pixels and is mapped onto the given display bounds
// (in global points).
void macVncInputInit( CGDirectDisplayID display, CGRect displayBounds, int fbWidth, int fbHeight );

// Inject a pointer event coming from the RFB protocol.
void macVncInjectPointer( int buttonMask, int x, int y );

// Map a point in global display coordinates (points) onto the framebuffer
// (pixels) - the inverse of what macVncInjectPointer() does with the
// coordinates it receives. Points outside the captured display are clamped to
// its edge. Returns false before macVncInputInit() has run.
bool macVncInputMapToFramebuffer( CGPoint point, int* x, int* y );

// Inject a keyboard event coming from the RFB protocol (keysym is an X keysym).
void macVncInjectKey( bool down, uint32_t keysym );
