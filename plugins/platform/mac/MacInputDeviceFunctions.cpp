/*
 * MacInputDeviceFunctions.cpp - implementation of MacInputDeviceFunctions class
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

#include "MacInputDeviceFunctions.h"
#include "MacKeyboardShortcutTrapper.h"


void MacInputDeviceFunctions::enableInputDevices()
{
	if( m_inputDevicesDisabled )
	{
		// TODO: re-enable local input via CGEvent / HID interface
		m_inputDevicesDisabled = false;
	}
}



void MacInputDeviceFunctions::disableInputDevices()
{
	if( m_inputDevicesDisabled == false )
	{
		// TODO: block local keyboard/mouse input. On macOS this requires either
		// CGEventTapCreate with an active run loop or an Accessibility-privileged
		// helper. Not yet implemented.
		m_inputDevicesDisabled = true;
	}
}



KeyboardShortcutTrapper* MacInputDeviceFunctions::createKeyboardShortcutTrapper( QObject* parent )
{
	return new MacKeyboardShortcutTrapper( parent );
}
