/*
 * AppMonitorController.cpp - running applications of one computer
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

#include <QCollator>
#include <QDateTime>

#include "AppMonitorController.h"
#include "ApplicationMonitoringPlugin.h"


AppMonitorController::AppMonitorController( ComputerGridModel* computers, QObject* parent ) :
	FeatureSession( QStringLiteral("ApplicationMonitoring"), computers, parent )
{
	// same refresh rate as the desktop dialog
	m_pollTimer.setInterval( 3000 );
	connect( &m_pollTimer, &QTimer::timeout, this, &AppMonitorController::poll );

	// computers without the plugin (e.g. Linux) never answer
	m_timeoutTimer.setSingleShot( true );
	m_timeoutTimer.setInterval( 8000 );
	connect( &m_timeoutTimer, &QTimer::timeout, this, [this]() {
		if( m_state == QLatin1String("loading") )
		{
			setState( QStringLiteral("noresponse") );
		}
	} );

	connect( this, &FeatureSession::onlineChanged, this, [this]() {
		if( computerUid().isEmpty() == false )
		{
			refresh();
		}
	} );
}



void AppMonitorController::open( const QString& uid )
{
	if( uid != computerUid() )
	{
		m_applications.clear();
		m_frontmost.clear();
		m_updatedAt.clear();
		Q_EMIT applicationsChanged();
		setState( {} );
	}

	setComputer( uid );
	refresh();
	m_pollTimer.start();
}



void AppMonitorController::close()
{
	m_pollTimer.stop();
	m_timeoutTimer.stop();
	setComputer( {} );
}



void AppMonitorController::refresh()
{
	if( isOnline() == false )
	{
		setState( QStringLiteral("offline") );
		return;
	}

	if( m_state != QLatin1String("ready") )
	{
		setState( QStringLiteral("loading") );
		m_timeoutTimer.start();
	}
	poll();
}



bool AppMonitorController::terminate( const QString& application )
{
	return sendToComputer( FeatureMessage{ featureUid(), ApplicationMonitoringPlugin::TerminateApplication }
							   .addArgument( ApplicationMonitoringPlugin::Argument::TargetApplication, application ) );
}



void AppMonitorController::handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( message.command<ApplicationMonitoringPlugin::Command>() != ApplicationMonitoringPlugin::ApplicationsReply ||
		uidOf( controlInterface ) != computerUid() )
	{
		return;
	}

	auto applications = message.argument( ApplicationMonitoringPlugin::Argument::Applications ).toString()
							.split( QLatin1Char('\n'), Qt::SkipEmptyParts );
	applications.removeDuplicates();

	QCollator collator;
	collator.setCaseSensitivity( Qt::CaseInsensitive );
	std::sort( applications.begin(), applications.end(), collator );

	m_applications = applications;
	m_frontmost = message.argument( ApplicationMonitoringPlugin::Argument::Frontmost ).toString();
	m_updatedAt = QDateTime::currentDateTime().toString( QStringLiteral("HH:mm:ss") );
	m_timeoutTimer.stop();

	Q_EMIT applicationsChanged();
	setState( QStringLiteral("ready") );
}



void AppMonitorController::poll()
{
	if( sendToComputer( FeatureMessage{ featureUid(), ApplicationMonitoringPlugin::RequestApplications } ) == false )
	{
		setState( QStringLiteral("offline") );
	}
}



void AppMonitorController::setState( const QString& state )
{
	if( state != m_state )
	{
		m_state = state;
		Q_EMIT stateChanged();
	}
}
