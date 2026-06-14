/*
 * ApplicationMonitoringPlugin.cpp - implementation of ApplicationMonitoringPlugin
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

#include "ApplicationMonitoringPlugin.h"
#include "ApplicationListDialog.h"
#include "ComputerControlInterface.h"
#include "ApplicationList.h"
#include "VeyonServerInterface.h"


ApplicationMonitoringPlugin::ApplicationMonitoringPlugin( QObject* parent ) :
	QObject( parent ),
	m_applicationMonitoringFeature( Feature( QStringLiteral( "ApplicationMonitoring" ),
											 Feature::Flag::Action | Feature::Flag::AllComponents,
											 Feature::Uid( "3e7b1f95-0c6a-4d28-9b14-5f8e2a6d0c73" ),
											 Feature::Uid(),
											 tr( "Application monitoring" ), {},
											 tr( "Use this function to see which applications are "
												 "running on the selected computers." ),
											 QStringLiteral(":/appmonitoring/appmon.png") ) ),
	m_features( { m_applicationMonitoringFeature } )
{
}



ApplicationMonitoringPlugin::~ApplicationMonitoringPlugin()
{
	for( const auto& dialog : std::as_const( m_dialogs ) )
	{
		delete dialog;
	}
}



const FeatureList& ApplicationMonitoringPlugin::featureList() const
{
	return m_features;
}



ApplicationListDialog* ApplicationMonitoringPlugin::dialogFor( const ComputerControlInterface::Pointer& controlInterface )
{
	auto dialog = m_dialogs.value( controlInterface.data() ).data();
	if( dialog == nullptr )
	{
		dialog = new ApplicationListDialog( controlInterface->computer().displayName() );
		m_dialogs[controlInterface.data()] = dialog;

		const auto controlInterfaceCopy = controlInterface;
		connect( dialog, &ApplicationListDialog::refreshRequested, this, [this, controlInterfaceCopy]() {
			sendFeatureMessage( FeatureMessage{ m_applicationMonitoringFeature.uid(), RequestApplications },
								{ controlInterfaceCopy } );
		} );

		connect( dialog, &ApplicationListDialog::terminateRequested, this,
				 [this, controlInterfaceCopy]( const QString& application ) {
			sendFeatureMessage( FeatureMessage{ m_applicationMonitoringFeature.uid(), TerminateApplication }
									.addArgument( Argument::TargetApplication, application ),
								{ controlInterfaceCopy } );
		} );
	}
	return dialog;
}



bool ApplicationMonitoringPlugin::controlFeature( Feature::Uid featureUid, Operation operation,
												  const QVariantMap& arguments,
												  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( featureUid != m_applicationMonitoringFeature.uid() || operation != Operation::Start )
	{
		return false;
	}

	sendFeatureMessage( FeatureMessage{ featureUid, RequestApplications }, computerControlInterfaces );
	return true;
}



bool ApplicationMonitoringPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
												const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(master)

	if( feature.uid() != m_applicationMonitoringFeature.uid() )
	{
		return false;
	}

	for( const auto& controlInterface : computerControlInterfaces )
	{
		auto dialog = dialogFor( controlInterface );
		dialog->show();
		dialog->raise();
		dialog->activateWindow();
	}

	controlFeature( feature.uid(), Operation::Start, {}, computerControlInterfaces );
	return true;
}



bool ApplicationMonitoringPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
														const FeatureMessage& message )
{
	if( message.featureUid() != m_applicationMonitoringFeature.uid() ||
		static_cast<int>( message.command() ) != ApplicationsReply )
	{
		return false;
	}

	if( m_dialogs.contains( computerControlInterface.data() ) == false )
	{
		return true; // dialog already closed
	}

	auto dialog = dialogFor( computerControlInterface );
	const auto applications = message.argument( Argument::Applications ).toString()
								  .split( QLatin1Char('\n'), Qt::SkipEmptyParts );
	dialog->setApplications( applications, message.argument( Argument::Frontmost ).toString() );
	return true;
}



bool ApplicationMonitoringPlugin::handleFeatureMessage( VeyonServerInterface& server,
														const MessageContext& messageContext,
														const FeatureMessage& message )
{
	if( message.featureUid() != m_applicationMonitoringFeature.uid() )
	{
		return false;
	}

	const auto command = static_cast<int>( message.command() );

	if( command == TerminateApplication )
	{
		terminateApplication( message.argument( Argument::TargetApplication ).toString() );
		// fall through and reply with a fresh list so the master UI updates at once
	}
	else if( command != RequestApplications )
	{
		return false;
	}

	return server.sendFeatureMessageReply(
				messageContext,
				FeatureMessage{ m_applicationMonitoringFeature.uid(), ApplicationsReply }
					.addArgument( Argument::Applications, runningApplications().join( QLatin1Char('\n') ) )
					.addArgument( Argument::Frontmost, frontmostApplication() ) );
}
