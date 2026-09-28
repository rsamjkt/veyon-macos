/*
 * ApplicationList.h - list running GUI applications (platform-neutral interface)
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

#pragma once

#include <QStringList>

// Names of the running regular (GUI) applications in the current user session.
// Implemented per platform (NSWorkspace on macOS, EnumWindows/psapi on Windows).
QStringList runningApplications();

// Name of the currently focused (frontmost / foreground) application.
QString frontmostApplication();

// Terminate every running process whose display name matches `name`.
void terminateApplication( const QString& name );

// Seconds since the last keyboard/mouse input in the current user session.
int secondsSinceLastInput();
