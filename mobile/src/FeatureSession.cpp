/*
 * FeatureSession.cpp - base for interactive per-computer features of the mobile UI
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

#include "ComputerGridModel.h"
#include "FeatureManager.h"
#include "FeatureSession.h"


FeatureSession::FeatureSession( const QString& featureName, ComputerGridModel* computers, QObject* parent ) :
	QObject( parent ),
	m_computers( computers )
{
	for( const auto& feature : VeyonCore::featureManager().features() )
	{
		if( feature.name() == featureName )
		{
			m_featureUid = feature.uid();
			break;
		}
	}

	if( isAvailable() )
	{
		connect( &VeyonCore::featureManager(), &FeatureManager::featureMessageReceived, this,
				 [this]( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) {
			if( message.featureUid() == m_featureUid && controlInterface.isNull() == false )
			{
				handleMessage( controlInterface, message );
			}
		} );
	}
}



QString FeatureSession::computerName() const
{
	const auto controlInterface = this->controlInterface();
	return controlInterface ? controlInterface->computerName() : QString{};
}



bool FeatureSession::isOnline() const
{
	const auto controlInterface = this->controlInterface();
	return controlInterface && controlInterface->state() == ComputerControlInterface::State::Connected;
}



void FeatureSession::setComputer( const QString& uid )
{
	if( uid == m_computerUid )
	{
		return;
	}

	m_computerUid = uid;

	disconnect( m_stateConnection );
	m_watchedInterface = controlInterface().data();
	if( m_watchedInterface )
	{
		m_stateConnection = connect( m_watchedInterface, &ComputerControlInterface::stateChanged,
									 this, &FeatureSession::onlineChanged );
	}

	Q_EMIT computerChanged();
	Q_EMIT onlineChanged();
}



ComputerControlInterface::Pointer FeatureSession::controlInterface() const
{
	return m_computerUid.isEmpty() ? ComputerControlInterface::Pointer{} : m_computers->controlInterface( m_computerUid );
}



bool FeatureSession::sendToComputer( const FeatureMessage& message )
{
	return send( controlInterface(), message );
}



bool FeatureSession::send( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( controlInterface.isNull() || controlInterface->state() != ComputerControlInterface::State::Connected )
	{
		return false;
	}

	controlInterface->sendFeatureMessage( message );
	return true;
}



QString FeatureSession::uidOf( const ComputerControlInterface::Pointer& controlInterface )
{
	return controlInterface ? controlInterface->computer().networkObjectUid().toString() : QString{};
}
