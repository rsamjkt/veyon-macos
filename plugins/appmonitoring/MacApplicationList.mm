/*
 * MacApplicationList.mm - list running GUI applications via NSWorkspace
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

#import <AppKit/AppKit.h>

#include "ApplicationList.h"


QStringList runningApplications()
{
	QStringList applications;

	@autoreleasepool {
		for( NSRunningApplication* app in [[NSWorkspace sharedWorkspace] runningApplications] )
		{
			if( app.activationPolicy == NSApplicationActivationPolicyRegular && app.localizedName != nil )
			{
				applications << QString::fromNSString( app.localizedName );
			}
		}
	}

	applications.removeDuplicates();
	applications.sort( Qt::CaseInsensitive );
	return applications;
}



QString frontmostApplication()
{
	QString name;

	@autoreleasepool {
		NSRunningApplication* frontmost = [[NSWorkspace sharedWorkspace] frontmostApplication];
		if( frontmost != nil && frontmost.localizedName != nil )
		{
			name = QString::fromNSString( frontmost.localizedName );
		}
	}

	return name;
}



void terminateApplication( const QString& name )
{
	@autoreleasepool {
		for( NSRunningApplication* app in [[NSWorkspace sharedWorkspace] runningApplications] )
		{
			if( app.localizedName != nil && QString::fromNSString( app.localizedName ) == name )
			{
				if( [app terminate] == NO )
				{
					[app forceTerminate];
				}
			}
		}
	}
}
