/*
 * MacVncInput.cpp - remote input injection for the macOS VNC server
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

#include "MacVncInput.h"

namespace {

struct InputState
{
	CGDirectDisplayID display{0};
	CGRect bounds{};
	int fbWidth{0};
	int fbHeight{0};
	int lastButtonMask{0};
	CGEventFlags flags{0};
};

InputState g;


CGPoint mapPoint( int x, int y )
{
	CGPoint p;
	const double fx = g.fbWidth > 0 ? static_cast<double>(x) / g.fbWidth : 0.0;
	const double fy = g.fbHeight > 0 ? static_cast<double>(y) / g.fbHeight : 0.0;
	p.x = g.bounds.origin.x + fx * g.bounds.size.width;
	p.y = g.bounds.origin.y + fy * g.bounds.size.height;
	return p;
}


void postMouse( CGEventType type, CGPoint pt, CGMouseButton button )
{
	CGEventRef e = CGEventCreateMouseEvent( nullptr, type, pt, button );
	if( e )
	{
		CGEventSetFlags( e, g.flags );
		CGEventPost( kCGHIDEventTap, e );
		CFRelease( e );
	}
}


void postKey( CGKeyCode keycode, bool down )
{
	CGEventRef e = CGEventCreateKeyboardEvent( nullptr, keycode, down );
	if( e )
	{
		CGEventSetFlags( e, g.flags );
		CGEventPost( kCGHIDEventTap, e );
		CFRelease( e );
	}
}


void postUnicode( UniChar ch )
{
	CGEventRef down = CGEventCreateKeyboardEvent( nullptr, 0, true );
	if( down )
	{
		CGEventKeyboardSetUnicodeString( down, 1, &ch );
		CGEventSetFlags( down, 0 );
		CGEventPost( kCGHIDEventTap, down );
		CFRelease( down );
	}

	CGEventRef up = CGEventCreateKeyboardEvent( nullptr, 0, false );
	if( up )
	{
		CGEventKeyboardSetUnicodeString( up, 1, &ch );
		CGEventSetFlags( up, 0 );
		CGEventPost( kCGHIDEventTap, up );
		CFRelease( up );
	}
}


// Map an X keysym for a modifier key to the corresponding CGEventFlags bit.
CGEventFlags modifierBit( uint32_t keysym )
{
	switch( keysym )
	{
	case 0xffe1: // Shift_L
	case 0xffe2: // Shift_R
		return kCGEventFlagMaskShift;
	case 0xffe3: // Control_L
	case 0xffe4: // Control_R
		return kCGEventFlagMaskControl;
	case 0xffe9: // Alt_L
	case 0xffea: // Alt_R
	case 0xfe03: // ISO_Level3_Shift (AltGr)
		return kCGEventFlagMaskAlternate;
	case 0xffe7: // Meta_L
	case 0xffe8: // Meta_R
	case 0xffeb: // Super_L
	case 0xffec: // Super_R
		return kCGEventFlagMaskCommand;
	default:
		return 0;
	}
}


CGKeyCode modifierKeyCode( uint32_t keysym )
{
	switch( keysym )
	{
	case 0xffe1: case 0xffe2: return 56; // Shift
	case 0xffe3: case 0xffe4: return 59; // Control
	case 0xffe9: case 0xffea: case 0xfe03: return 58; // Option
	case 0xffe7: case 0xffe8: case 0xffeb: case 0xffec: return 55; // Command
	default: return 0xFFFF;
	}
}


// Non-printable special keys -> US CGKeyCode, or 0xFFFF if not a special key.
CGKeyCode specialKeyCode( uint32_t keysym )
{
	switch( keysym )
	{
	case 0xff0d: return 36;  // Return
	case 0xff8d: return 76;  // KP_Enter
	case 0xff09: return 48;  // Tab
	case 0xff08: return 51;  // BackSpace
	case 0xffff: return 117; // Delete (forward)
	case 0xff1b: return 53;  // Escape
	case 0xff51: return 123; // Left
	case 0xff53: return 124; // Right
	case 0xff54: return 125; // Down
	case 0xff52: return 126; // Up
	case 0xff50: return 115; // Home
	case 0xff57: return 119; // End
	case 0xff55: return 116; // Page Up
	case 0xff56: return 121; // Page Down
	case 0xffe5: return 57;  // Caps Lock
	case 0xffbe: return 122; // F1
	case 0xffbf: return 120; // F2
	case 0xffc0: return 99;  // F3
	case 0xffc1: return 118; // F4
	case 0xffc2: return 96;  // F5
	case 0xffc3: return 97;  // F6
	case 0xffc4: return 98;  // F7
	case 0xffc5: return 100; // F8
	case 0xffc6: return 101; // F9
	case 0xffc7: return 109; // F10
	case 0xffc8: return 103; // F11
	case 0xffc9: return 111; // F12
	default: return 0xFFFF;
	}
}


// Letters/digits -> US CGKeyCode (used in shortcut context), or 0xFFFF.
CGKeyCode letterDigitKeyCode( uint32_t keysym )
{
	// normalise upper-case ASCII letters to lower case
	if( keysym >= 'A' && keysym <= 'Z' )
	{
		keysym = keysym - 'A' + 'a';
	}

	switch( keysym )
	{
	case 'a': return 0;   case 'b': return 11;  case 'c': return 8;
	case 'd': return 2;   case 'e': return 14;  case 'f': return 3;
	case 'g': return 5;   case 'h': return 4;   case 'i': return 34;
	case 'j': return 38;  case 'k': return 40;  case 'l': return 37;
	case 'm': return 46;  case 'n': return 45;  case 'o': return 31;
	case 'p': return 35;  case 'q': return 12;  case 'r': return 15;
	case 's': return 1;   case 't': return 17;  case 'u': return 32;
	case 'v': return 9;   case 'w': return 13;  case 'x': return 7;
	case 'y': return 16;  case 'z': return 6;
	case '0': return 29;  case '1': return 18;  case '2': return 19;
	case '3': return 20;  case '4': return 21;  case '5': return 23;
	case '6': return 22;  case '7': return 26;  case '8': return 28;
	case '9': return 25;
	case ' ': return 49;  // Space
	default: return 0xFFFF;
	}
}


// Derive a printable Unicode character from an X keysym (BMP only), or 0.
UniChar keysymToUnicode( uint32_t keysym )
{
	if( keysym >= 0x20 && keysym <= 0x7e )
	{
		return static_cast<UniChar>( keysym );
	}
	if( keysym >= 0xa0 && keysym <= 0xff )
	{
		return static_cast<UniChar>( keysym );
	}
	if( keysym >= 0xffb0 && keysym <= 0xffb9 ) // keypad 0-9
	{
		return static_cast<UniChar>( '0' + ( keysym - 0xffb0 ) );
	}
	// direct Unicode keysyms: 0x01000000 | codepoint
	if( keysym >= 0x01000000 && keysym <= 0x0100ffff )
	{
		return static_cast<UniChar>( keysym & 0xffff );
	}
	return 0;
}

} // namespace



void macVncInputInit( CGDirectDisplayID display, CGRect displayBounds, int fbWidth, int fbHeight )
{
	g.display = display;
	g.bounds = displayBounds;
	g.fbWidth = fbWidth;
	g.fbHeight = fbHeight;
	g.lastButtonMask = 0;
	g.flags = 0;
}



void macVncInjectPointer( int buttonMask, int x, int y )
{
	const CGPoint pt = mapPoint( x, y );
	const int prev = g.lastButtonMask;

	const bool leftPrev = prev & 0x1, leftNow = buttonMask & 0x1;
	const bool midPrev = prev & 0x2, midNow = buttonMask & 0x2;
	const bool rightPrev = prev & 0x4, rightNow = buttonMask & 0x4;

	// position update first (drag if a button is currently held)
	if( leftPrev )
	{
		postMouse( kCGEventLeftMouseDragged, pt, kCGMouseButtonLeft );
	}
	else if( rightPrev )
	{
		postMouse( kCGEventRightMouseDragged, pt, kCGMouseButtonRight );
	}
	else if( midPrev )
	{
		postMouse( kCGEventOtherMouseDragged, pt, kCGMouseButtonCenter );
	}
	else
	{
		postMouse( kCGEventMouseMoved, pt, kCGMouseButtonLeft );
	}

	// button transitions
	if( leftNow && !leftPrev ) { postMouse( kCGEventLeftMouseDown, pt, kCGMouseButtonLeft ); }
	else if( !leftNow && leftPrev ) { postMouse( kCGEventLeftMouseUp, pt, kCGMouseButtonLeft ); }

	if( rightNow && !rightPrev ) { postMouse( kCGEventRightMouseDown, pt, kCGMouseButtonRight ); }
	else if( !rightNow && rightPrev ) { postMouse( kCGEventRightMouseUp, pt, kCGMouseButtonRight ); }

	if( midNow && !midPrev ) { postMouse( kCGEventOtherMouseDown, pt, kCGMouseButtonCenter ); }
	else if( !midNow && midPrev ) { postMouse( kCGEventOtherMouseUp, pt, kCGMouseButtonCenter ); }

	// scroll wheel: RFB encodes wheel up/down as buttons 4 and 5 (bits 3 and 4)
	const bool wheelUp = ( buttonMask & 0x8 ) && !( prev & 0x8 );
	const bool wheelDown = ( buttonMask & 0x10 ) && !( prev & 0x10 );
	if( wheelUp || wheelDown )
	{
		const int32_t delta = wheelUp ? 3 : -3;
		CGEventRef scroll = CGEventCreateScrollWheelEvent( nullptr, kCGScrollEventUnitLine, 1, delta );
		if( scroll )
		{
			CGEventSetFlags( scroll, g.flags );
			CGEventPost( kCGHIDEventTap, scroll );
			CFRelease( scroll );
		}
	}

	g.lastButtonMask = buttonMask;
}



void macVncInjectKey( bool down, uint32_t keysym )
{
	// modifier keys
	const CGEventFlags modBit = modifierBit( keysym );
	if( modBit )
	{
		if( down )
		{
			g.flags |= modBit;
		}
		else
		{
			g.flags &= ~modBit;
		}
		const CGKeyCode kc = modifierKeyCode( keysym );
		if( kc != 0xFFFF )
		{
			postKey( kc, down );
		}
		return;
	}

	// special non-printable keys -> always via key code
	CGKeyCode kc = specialKeyCode( keysym );

	const bool shortcut = ( g.flags & ( kCGEventFlagMaskCommand |
										kCGEventFlagMaskControl |
										kCGEventFlagMaskAlternate ) ) != 0;

	// in a shortcut context use the physical key code so combos like Cmd+C work
	if( kc == 0xFFFF && shortcut )
	{
		kc = letterDigitKeyCode( keysym );
	}

	if( kc != 0xFFFF )
	{
		postKey( kc, down );
		return;
	}

	// printable character: synthesise via Unicode on the key-down edge only
	if( down )
	{
		const UniChar ch = keysymToUnicode( keysym );
		if( ch )
		{
			postUnicode( ch );
		}
	}
}
