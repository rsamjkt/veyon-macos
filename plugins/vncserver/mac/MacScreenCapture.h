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

// Resolve the SCDisplay for the given display and prepare a content filter.
// Returns the capture dimensions (in points) via outWidth/outHeight.
// Returns false if ScreenCaptureKit is unavailable or the Screen Recording
// permission has not been granted.
bool macScreenCaptureInit( CGDirectDisplayID display, int* outWidth, int* outHeight );

// Capture a single frame into the given QImage (must be Format_RGB32 and sized
// to the dimensions returned by macScreenCaptureInit). Returns false on failure.
bool macScreenCaptureFrame( QImage& target );

// Release ScreenCaptureKit resources.
void macScreenCaptureCleanup();
