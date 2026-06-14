/*
 * InternetAccessFeaturePlugin.cpp - implementation of InternetAccessFeaturePlugin
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

#include <QFile>

#include "InternetAccessFeaturePlugin.h"
#include "PlatformCoreFunctions.h"
#include "PlatformFilesystemFunctions.h"
#include "VeyonServerInterface.h"


// pf ruleset that blocks outbound traffic to the public internet while keeping
// loopback and private/LAN ranges reachable (so AruniControl itself keeps working).
static const char* const blockRuleset =
	"set block-policy drop\n"
	"table <aruni_allowed> persist { 127.0.0.0/8 10.0.0.0/8 172.16.0.0/12 "
	"192.168.0.0/16 169.254.0.0/16 224.0.0.0/4 }\n"
	"block drop out all\n"
	"pass out quick to <aruni_allowed>\n"
	"pass out quick proto udp from any to any port { 67, 68 }\n";


InternetAccessFeaturePlugin::InternetAccessFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_internetAccessFeature( Feature( QStringLiteral( "InternetAccessControl" ),
									  Feature::Flag::Mode | Feature::Flag::AllComponents,
									  Feature::Uid( "6f3a1d27-9b40-4c85-a216-0e8d5f7b3c91" ),
									  Feature::Uid(),
									  tr( "Block internet" ),
									  tr( "Allow internet" ),
									  tr( "Use this function to block or allow internet access on "
										  "the selected computers, e.g. during exams." ),
									  QStringLiteral(":/internetaccess/no-internet.png") ) ),
	m_features( { m_internetAccessFeature } )
{
}



const FeatureList& InternetAccessFeaturePlugin::featureList() const
{
	return m_features;
}



bool InternetAccessFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
												  const QVariantMap& arguments,
												  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( featureUid != m_internetAccessFeature.uid() )
	{
		return false;
	}

	const auto command = ( operation == Operation::Start ) ? BlockInternet : AllowInternet;
	sendFeatureMessage( FeatureMessage{ featureUid, command }, computerControlInterfaces );
	return true;
}



bool InternetAccessFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
														const MessageContext& messageContext,
														const FeatureMessage& message )
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if( message.featureUid() != m_internetAccessFeature.uid() )
	{
		return false;
	}

	applyInternetBlock( static_cast<int>( message.command() ) == BlockInternet );
	return true;
}



bool InternetAccessFeaturePlugin::applyInternetBlock( bool blocked )
{
	auto& core = VeyonCore::platform().coreFunctions();

	if( blocked )
	{
		const auto rulesetPath = VeyonCore::platform().filesystemFunctions().globalTempPath() +
								 QStringLiteral("/aruni-internet-block.conf");
		QFile file( rulesetPath );
		if( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) == false )
		{
			vCritical() << "InternetAccessControl: cannot write ruleset" << rulesetPath;
			return false;
		}
		file.write( blockRuleset );
		file.close();

		// requires administrator rights on the client (pf firewall). For an
		// unattended classroom deployment, grant pfctl passwordless sudo or run
		// the AruniControl service as a privileged LaunchDaemon.
		return core.runProgramAsAdmin( QStringLiteral("/sbin/pfctl"),
									   { QStringLiteral("-E"), QStringLiteral("-f"), rulesetPath } );
	}

	// restore the default macOS packet filter ruleset
	return core.runProgramAsAdmin( QStringLiteral("/sbin/pfctl"),
								   { QStringLiteral("-f"), QStringLiteral("/etc/pf.conf") } );
}
