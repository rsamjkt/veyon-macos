/*
 * AutoUpdatePlugin.cpp - automatic updates of AruniControl
 *
 * Copyright (c) 2026 AruniControl Community
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

#include <QTimer>

#include "AutoUpdateConfigurationPage.h"
#include "AutoUpdatePlugin.h"
#include "Updater.h"
#include "VeyonCore.h"


AutoUpdatePlugin::AutoUpdatePlugin( QObject* parent ) :
	QObject( parent )
{
	// the always-running server does the updates (on Windows it runs as
	// SYSTEM and may start the installer)
	QTimer::singleShot( 0, this, [this]() {
		if( VeyonCore::component() == VeyonCore::Component::Server )
		{
			m_updater = new Updater( this );
		}
	} );
}



ConfigurationPage* AutoUpdatePlugin::createConfigurationPage()
{
	return new AutoUpdateConfigurationPage;
}
