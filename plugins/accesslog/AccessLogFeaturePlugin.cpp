/*
 * AccessLogFeaturePlugin.cpp - who accessed this computer, when and how
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

#include <QJsonDocument>

#include "AccessLog.h"
#include "AccessLogDialog.h"
#include "AccessLogFeaturePlugin.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


AccessLogFeaturePlugin::AccessLogFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_accessLogFeature( Feature( QStringLiteral( "AccessLog" ),
								 Feature::Flag::Action | Feature::Flag::AllComponents,
								 Feature::Uid( FeatureUid ),
								 Feature::Uid(),
								 tr( "Access log" ), {},
								 tr( "Show who accessed the selected computers, when and with which functions." ),
								 QStringLiteral(":/accesslog/accesslog.png") ) ),
	m_features( { m_accessLogFeature } )
{
}



const FeatureList& AccessLogFeaturePlugin::featureList() const
{
	return m_features;
}



bool AccessLogFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											 const QVariantMap& arguments,
											 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_accessLogFeature.uid() || operation != Operation::Initialize )
	{
		return false;
	}

	sendFeatureMessage( FeatureMessage{ featureUid, Query }
							.addArgument( Argument::Since, arguments.value( QStringLiteral("since") ).toString() )
							.addArgument( Argument::Limit, arguments.value( QStringLiteral("limit"), 1000 ).toInt() ),
						computerControlInterfaces );
	return true;
}



bool AccessLogFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_accessLogFeature.uid() || computerControlInterfaces.isEmpty() )
	{
		return false;
	}

	auto dialog = new AccessLogDialog( this, computerControlInterfaces, master.mainWindow() );
	dialog->setAttribute( Qt::WA_DeleteOnClose );
	dialog->show();

	controlFeature( feature.uid(), Operation::Initialize, { { QStringLiteral("limit"), 2000 } }, computerControlInterfaces );
	return true;
}



bool AccessLogFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												   const FeatureMessage& message )
{
	if( message.featureUid() != m_accessLogFeature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Entries )
	{
		Q_EMIT entriesReceived( computerControlInterface,
								QJsonDocument::fromJson( message.argument( Argument::Entries ).toByteArray() ).array() );
	}
	return true;
}



bool AccessLogFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
												   const FeatureMessage& message )
{
	if( message.featureUid() != m_accessLogFeature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Query )
	{
		const auto since = QDateTime::fromString( message.argument( Argument::Since ).toString(), Qt::ISODate );
		const auto limit = qBound( 1, message.argument( Argument::Limit ).toInt(), 5000 );
		return server.sendFeatureMessageReply( messageContext,
			FeatureMessage{ m_accessLogFeature.uid(), Entries }
				.addArgument( Argument::Entries, QJsonDocument( AccessLog::read( since, limit ) ).toJson( QJsonDocument::Compact ) ) );
	}

	return true;
}
