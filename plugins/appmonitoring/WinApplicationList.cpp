/*
 * WinApplicationList.cpp - list running GUI applications via the Win32 API
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

#include <QSet>

#include "ApplicationList.h"

#include <windows.h>
#include <tlhelp32.h>
#include <cwchar>


// Resolve a human-friendly application name for a process: prefer the
// executable's version-info FileDescription (e.g. "Google Chrome"), and fall
// back to the executable base name without extension (e.g. "chrome").
static QString friendlyNameForProcess( DWORD processId )
{
	QString result;

	HANDLE process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId );
	if( process == nullptr )
	{
		return result;
	}

	wchar_t path[MAX_PATH] = {};
	DWORD pathSize = MAX_PATH;
	if( QueryFullProcessImageNameW( process, 0, path, &pathSize ) )
	{
		DWORD versionHandle = 0;
		const DWORD infoSize = GetFileVersionInfoSizeW( path, &versionHandle );
		if( infoSize > 0 )
		{
			QByteArray buffer( static_cast<int>( infoSize ), Qt::Uninitialized );
			if( GetFileVersionInfoW( path, versionHandle, infoSize, buffer.data() ) )
			{
				struct LangAndCodePage { WORD language; WORD codePage; } *translation = nullptr;
				UINT translationLen = 0;
				if( VerQueryValueW( buffer.data(), L"\\VarFileInfo\\Translation",
									reinterpret_cast<LPVOID*>( &translation ), &translationLen ) &&
					translationLen >= sizeof( LangAndCodePage ) )
				{
					wchar_t subBlock[64] = {};
					swprintf( subBlock, 64, L"\\StringFileInfo\\%04x%04x\\FileDescription",
							  translation->language, translation->codePage );

					wchar_t* description = nullptr;
					UINT descriptionLen = 0;
					if( VerQueryValueW( buffer.data(), subBlock,
										reinterpret_cast<LPVOID*>( &description ), &descriptionLen ) &&
						descriptionLen > 1 )
					{
						result = QString::fromWCharArray( description ).trimmed();
					}
				}
			}
		}

		if( result.isEmpty() )
		{
			QString executable = QString::fromWCharArray( path );
			const int slash = executable.lastIndexOf( QLatin1Char( '\\' ) );
			if( slash >= 0 )
			{
				executable = executable.mid( slash + 1 );
			}
			if( executable.endsWith( QStringLiteral( ".exe" ), Qt::CaseInsensitive ) )
			{
				executable.chop( 4 );
			}
			result = executable;
		}
	}

	CloseHandle( process );
	return result;
}



namespace {

struct EnumContext
{
	QStringList* names;
	QSet<QString>* seen;
};

BOOL CALLBACK enumWindowProc( HWND window, LPARAM lParam )
{
	if( IsWindowVisible( window ) == FALSE )
	{
		return TRUE;
	}

	// alt-tab-style filtering: keep titled top-level application windows,
	// drop tool windows and owned helper windows
	const LONG_PTR exStyle = GetWindowLongPtrW( window, GWL_EXSTYLE );
	if( exStyle & WS_EX_TOOLWINDOW )
	{
		return TRUE;
	}
	if( GetWindow( window, GW_OWNER ) != nullptr && ( exStyle & WS_EX_APPWINDOW ) == 0 )
	{
		return TRUE;
	}
	if( GetWindowTextLengthW( window ) <= 0 )
	{
		return TRUE;
	}

	DWORD processId = 0;
	GetWindowThreadProcessId( window, &processId );
	if( processId == 0 )
	{
		return TRUE;
	}

	const QString name = friendlyNameForProcess( processId );
	auto* context = reinterpret_cast<EnumContext*>( lParam );
	if( name.isEmpty() == false && context->seen->contains( name ) == false )
	{
		context->seen->insert( name );
		*context->names << name;
	}

	return TRUE;
}

}



QStringList runningApplications()
{
	QStringList applications;
	QSet<QString> seen;
	EnumContext context{ &applications, &seen };

	EnumWindows( enumWindowProc, reinterpret_cast<LPARAM>( &context ) );

	applications.sort( Qt::CaseInsensitive );
	return applications;
}



QString frontmostApplication()
{
	HWND window = GetForegroundWindow();
	if( window == nullptr )
	{
		return {};
	}

	DWORD processId = 0;
	GetWindowThreadProcessId( window, &processId );
	if( processId == 0 )
	{
		return {};
	}

	return friendlyNameForProcess( processId );
}



void terminateApplication( const QString& name )
{
	HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( snapshot == INVALID_HANDLE_VALUE )
	{
		return;
	}

	PROCESSENTRY32W entry;
	entry.dwSize = sizeof( entry );
	if( Process32FirstW( snapshot, &entry ) )
	{
		do
		{
			if( friendlyNameForProcess( entry.th32ProcessID ) == name )
			{
				HANDLE process = OpenProcess( PROCESS_TERMINATE, FALSE, entry.th32ProcessID );
				if( process != nullptr )
				{
					TerminateProcess( process, 1 );
					CloseHandle( process );
				}
			}
		}
		while( Process32NextW( snapshot, &entry ) );
	}

	CloseHandle( snapshot );
}
