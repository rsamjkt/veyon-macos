/*
 * GatewayPlugin.cpp - Aruni Gateway: reach this LAN's computers via the Aruni Relay
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

#include "GatewayConfigurationPage.h"
#include "GatewayPlugin.h"
#include "GatewayService.h"
#include "VeyonCore.h"


GatewayPlugin::GatewayPlugin( QObject* parent ) :
	QObject( parent )
{
	// the gateway lives in the always-running AruniControl server of a client
	// PC; it stays idle until enabled in the Configurator
	QTimer::singleShot( 0, this, [this]() {
		if( VeyonCore::component() == VeyonCore::Component::Server )
		{
			m_service = new GatewayService( this );
		}
	} );
}



ConfigurationPage* GatewayPlugin::createConfigurationPage()
{
	return new GatewayConfigurationPage;
}
