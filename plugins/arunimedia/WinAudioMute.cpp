/*
 * WinAudioMute.cpp - mute/unmute the default audio endpoint via WASAPI
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

// INITGUID instantiates the CLSID_/IID_ GUIDs used below in this translation
// unit, so no extra import library (uuid) is needed under MinGW.
#define INITGUID

#include "WinAudioMute.h"

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>


void setSystemAudioMutedWin( bool muted )
{
	const bool comInitialized = SUCCEEDED( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) );

	IMMDeviceEnumerator* enumerator = nullptr;
	if( SUCCEEDED( CoCreateInstance( CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
									 IID_IMMDeviceEnumerator,
									 reinterpret_cast<void**>( &enumerator ) ) ) )
	{
		IMMDevice* device = nullptr;
		if( SUCCEEDED( enumerator->GetDefaultAudioEndpoint( eRender, eConsole, &device ) ) )
		{
			IAudioEndpointVolume* endpointVolume = nullptr;
			if( SUCCEEDED( device->Activate( IID_IAudioEndpointVolume, CLSCTX_ALL, nullptr,
											 reinterpret_cast<void**>( &endpointVolume ) ) ) )
			{
				endpointVolume->SetMute( muted ? TRUE : FALSE, nullptr );
				endpointVolume->Release();
			}
			device->Release();
		}
		enumerator->Release();
	}

	if( comInitialized )
	{
		CoUninitialize();
	}
}
