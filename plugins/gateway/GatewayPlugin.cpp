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

#include <QCoreApplication>
#include <QJsonArray>
#include <QTimer>

#include "GatewayConfigurationPage.h"
#include "GatewayPlugin.h"
#include "GatewayService.h"
#include "RoamingAgent.h"
#include "VeyonCore.h"


GatewayPlugin::GatewayPlugin( QObject* parent ) :
	QObject( parent ),
	m_commands( {
		{ QStringLiteral("status"), tr( "Show the state of the gateway and of the roaming laptop mode" ) },
		{ QStringLiteral("enroll"), tr( "Keep this laptop reachable outside the office (enrollment code from the office gateway)" ) },
		{ QStringLiteral("leave"), tr( "Stop the roaming laptop mode" ) },
		{ QStringLiteral("enrollmentcode"), tr( "Print the enrollment code for roaming laptops (on the office gateway)" ) },
		{ QStringLiteral("runroaming"), tr( "Run the roaming laptop connection in the foreground (for testing)" ) },
	} )
{
	// the gateway and the roaming laptop mode live in the always-running
	// AruniControl server; both stay idle until enabled in the Configurator
	QTimer::singleShot( 0, this, [this]() {
		if( VeyonCore::component() == VeyonCore::Component::Server )
		{
			m_service = new GatewayService( this );
			m_roamingAgent = new RoamingAgent( this );
		}
	} );
}



QStringList GatewayPlugin::commands() const
{
	return m_commands.keys();
}



QString GatewayPlugin::commandHelp( const QString& command ) const
{
	return m_commands.value( command );
}



CommandLinePluginInterface::RunResult GatewayPlugin::handle_status( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	const auto state = GatewayState::load();
	const auto status = GatewayState::readStatus();
	const auto roaming = GatewayState::readStatus( QStringLiteral("roaming-status.json") );

	print( tr( "Aruni Gateway: %1" ).arg( state.enabled ? tr( "enabled" ) : tr( "disabled" ) ) );
	if( state.enabled )
	{
		print( tr( "  connected: %1" ).arg( status[QStringLiteral("connected")].toBool() ? tr( "yes" ) : tr( "no" ) ) );
		print( tr( "  roaming laptops: %1 registered, %2 online" )
				   .arg( state.agents.size() ).arg( status[QStringLiteral("agents")].toArray().size() ) );
	}

	print( tr( "Roaming laptop mode: %1" ).arg( state.roamingEnabled ? tr( "enabled" ) : tr( "disabled" ) ) );
	if( state.roamingEnabled )
	{
		print( tr( "  office: %1" ).arg( state.roamingHub.siteName ) );
		print( tr( "  connected: %1" ).arg( roaming[QStringLiteral("connected")].toBool() ? tr( "yes" ) : tr( "no" ) ) );
		const auto error = roaming[QStringLiteral("error")].toString();
		if( error.isEmpty() == false )
		{
			print( tr( "  last error: %1" ).arg( error ) );
		}
	}

	return NoResult;
}



CommandLinePluginInterface::RunResult GatewayPlugin::handle_enroll( const QStringList& arguments )
{
	if( arguments.isEmpty() )
	{
		return NotEnoughArguments;
	}

	const auto hub = AruniTunnel::PairingInfo::decode( arguments.first() );
	if( hub.enrollment == false || hub.isValid() == false )
	{
		error( tr( "This is not a valid enrollment code for roaming laptops." ) );
		return Failed;
	}

	bool isGateway = false;
	const bool saved = GatewayState::update( [&hub, &isGateway]( GatewayState& state ) {
		if( state.enabled )
		{
			isGateway = true;
			return;
		}
		state.roamingEnabled = true;
		state.roamingHub = hub;
		state.roamingRegistered = false;
	} );

	if( isGateway )
	{
		error( tr( "This computer is the Aruni Gateway itself and cannot be a roaming laptop." ) );
		return Failed;
	}

	if( saved == false )
	{
		error( tr( "Could not save the settings (run as administrator)." ) );
		return Failed;
	}

	info( tr( "This laptop stays reachable for \"%1\" outside the office." ).arg( hub.siteName ) );
	return Successful;
}



CommandLinePluginInterface::RunResult GatewayPlugin::handle_leave( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	const bool saved = GatewayState::update( []( GatewayState& state ) {
		state.roamingEnabled = false;
		state.roamingHub = {};
		state.roamingRegistered = false;
	} );

	return saved ? Successful : Failed;
}



CommandLinePluginInterface::RunResult GatewayPlugin::handle_enrollmentcode( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	// persist the identity so the printed code stays valid
	GatewayState::update( []( GatewayState& ) {} );
	const auto state = GatewayState::load();
	if( state.enabled == false )
	{
		warning( tr( "The Aruni Gateway is not enabled on this computer yet." ) );
	}

	print( state.enrollmentInfo().encode() );
	return NoResult;
}



CommandLinePluginInterface::RunResult GatewayPlugin::handle_runroaming( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	RoamingAgent agent;
	QCoreApplication::exec();
	return Successful;
}



ConfigurationPage* GatewayPlugin::createConfigurationPage()
{
	return new GatewayConfigurationPage;
}
