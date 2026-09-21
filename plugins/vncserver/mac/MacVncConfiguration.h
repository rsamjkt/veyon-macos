/*
 * MacVncConfiguration.h - macOS VNC server specific configuration values
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

#include "Configuration/Proxy.h"

// Settings of the macOS screen capture, all changeable through the CLI, e.g.
//
//   veyon-cli config set MacVncServer/CaptureScale 75
//
// CaptureScale is a percentage of the display's native pixel resolution: 100
// keeps every pixel (sharpest, the default), lower values trade sharpness for
// bandwidth on slow networks. CaptureFrameRate caps how many frames per second
// ScreenCaptureKit delivers. RemoteCursor keeps the mouse cursor out of the
// captured pixels and sends it as a VNC cursor shape instead, so moving the
// mouse no longer costs any framebuffer traffic.
#define FOREACH_MAC_VNC_CONFIG_PROPERTY(OP) \
	OP( MacVncConfiguration, m_configuration, int, captureScale, setCaptureScale, "CaptureScale", "MacVncServer", 100, Configuration::Property::Flag::Advanced ) \
	OP( MacVncConfiguration, m_configuration, int, captureFrameRate, setCaptureFrameRate, "CaptureFrameRate", "MacVncServer", 30, Configuration::Property::Flag::Advanced ) \
	OP( MacVncConfiguration, m_configuration, bool, remoteCursor, setRemoteCursor, "RemoteCursor", "MacVncServer", true, Configuration::Property::Flag::Advanced )

DECLARE_CONFIG_PROXY(MacVncConfiguration, FOREACH_MAC_VNC_CONFIG_PROPERTY)
