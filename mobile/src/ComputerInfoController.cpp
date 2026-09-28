/*
 * ComputerInfoController.cpp - hardware, disks and programs of one computer 
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
#include <QJsonObject>
#include <QLocale>
#include <QUuid>

#include "ComputerInfoController.h"
#include "FeatureManager.h"
#include "InventoryFeaturePlugin.h"
#include "SoftwareDeployFeaturePlugin.h"
#include "VeyonCore.h"


ComputerInfoController::ComputerInfoController( ComputerGridModel* computers, QObject* parent ) :
	FeatureSession( Feature::Uid( InventoryFeaturePlugin::FeatureUid ), computers, parent )
{
	m_timeoutTimer.setSingleShot( true );
	m_timeoutTimer.setInterval( 15000 );
	connect( &m_timeoutTimer, &QTimer::timeout, this, [this]() {
		if( m_state == QLatin1String("loading") )
		{
			setState( QStringLiteral("noresponse") );
		}
	} );

	// the programs come from the SoftwareDeploy plugin
	connect( &VeyonCore::featureManager(), &FeatureManager::featureMessageReceived, this,
			 [this]( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) {
		if( message.featureUid() == Feature::Uid( SoftwareDeployFeaturePlugin::FeatureUid ) && controlInterface.isNull() == false )
		{
			handleSoftwareMessage( controlInterface, message );
		}
	} );
}



void ComputerInfoController::open( const QString& uid )
{
	if( uid != computerUid() )
	{
		m_info.clear();
		m_disks.clear();
		m_network.clear();
		m_programs.clear();
		m_jobs.clear();
		Q_EMIT infoChanged();
		Q_EMIT programsChanged();
		setState( {} );
	}
	setComputer( uid );
	refresh();
}



void ComputerInfoController::close()
{
	m_timeoutTimer.stop();
	setComputer( {} );
}



void ComputerInfoController::refresh()
{
	if( computerUid().isEmpty() )
	{
		return;
	}
	if( sendToComputer( FeatureMessage{ featureUid(), InventoryFeaturePlugin::Query } ) == false )
	{
		setState( QStringLiteral("offline") );
		return;
	}
	send( controlInterface(), FeatureMessage{ Feature::Uid( SoftwareDeployFeaturePlugin::FeatureUid ), SoftwareDeployFeaturePlugin::List } );
	if( m_state != QLatin1String("ready") )
	{
		setState( QStringLiteral("loading") );
	}
	m_timeoutTimer.start();
}



bool ComputerInfoController::uninstall( const QString& programId )
{
	const auto job = QUuid::createUuid().toString( QUuid::WithoutBraces );
	if( send( controlInterface(), FeatureMessage{ Feature::Uid( SoftwareDeployFeaturePlugin::FeatureUid ), SoftwareDeployFeaturePlugin::Uninstall }
									  .addArgument( SoftwareDeployFeaturePlugin::Argument::Job, job )
									  .addArgument( SoftwareDeployFeaturePlugin::Argument::Program, programId )
									  .addArgument( SoftwareDeployFeaturePlugin::Argument::Arguments, QString{} ) ) == false )
	{
		return false;
	}
	m_jobs[job] = programId;
	setProgramState( programId, QStringLiteral("removing") );
	return true;
}



void ComputerInfoController::handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( static_cast<int>( message.command() ) != InventoryFeaturePlugin::Info || uidOf( controlInterface ) != computerUid() )
	{
		return;
	}
	m_timeoutTimer.stop();

	const auto d = QJsonDocument::fromJson( message.argument( InventoryFeaturePlugin::Argument::Inventory ).toByteArray() ).object();
	const QLocale locale;
	m_info = {
		{ QStringLiteral("model"), QStringLiteral("%1 %2").arg( d[QStringLiteral("manufacturer")].toString(), d[QStringLiteral("model")].toString() ).trimmed() },
		{ QStringLiteral("serial"), d[QStringLiteral("serial")].toString() },
		{ QStringLiteral("os"), d[QStringLiteral("os")].toString() },
		{ QStringLiteral("cpu"), QStringLiteral("%1 · %2 inti").arg( d[QStringLiteral("cpu")].toString() ).arg( d[QStringLiteral("cores")].toInt() ) },
		{ QStringLiteral("ram"), d.contains( QStringLiteral("ramMB") ) ? QStringLiteral("%1 GB").arg( qRound( d[QStringLiteral("ramMB")].toDouble() / 1024 ) ) : QString{} },
		{ QStringLiteral("user"), d[QStringLiteral("user")].toString() },
		{ QStringLiteral("uptime"), d.contains( QStringLiteral("uptimeHours") ) ? tr("%1 jam").arg( d[QStringLiteral("uptimeHours")].toInt() ) : QString{} },
		{ QStringLiteral("version"), d[QStringLiteral("version")].toString() },
	};

	m_disks.clear();
	for( const auto& value : d[QStringLiteral("disks")].toArray() )
	{
		const auto disk = value.toObject();
		const auto total = disk[QStringLiteral("totalGB")].toDouble();
		const auto free = disk[QStringLiteral("freeGB")].toDouble();
		m_disks.append( QVariantMap{
			{ QStringLiteral("path"), disk[QStringLiteral("path")].toString() },
			{ QStringLiteral("free"), QString( locale.toString( free, 'f', 1 ) + QStringLiteral(" GB") ) },
			{ QStringLiteral("total"), QString( locale.toString( total, 'f', 0 ) + QStringLiteral(" GB") ) },
			{ QStringLiteral("usedPercent"), total > 0 ? int( ( total - free ) * 100 / total ) : 0 },
		} );
	}

	m_network.clear();
	for( const auto& value : d[QStringLiteral("network")].toArray() )
	{
		const auto net = value.toObject();
		m_network.append( QVariantMap{ { QStringLiteral("name"), net[QStringLiteral("name")].toString() },
									   { QStringLiteral("ip"), net[QStringLiteral("ip")].toString() },
									   { QStringLiteral("mac"), net[QStringLiteral("mac")].toString() } } );
	}

	Q_EMIT infoChanged();
	setState( QStringLiteral("ready") );
}



void ComputerInfoController::handleSoftwareMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( uidOf( controlInterface ) != computerUid() )
	{
		return;
	}

	switch( static_cast<int>( message.command() ) )
	{
	case SoftwareDeployFeaturePlugin::Software:
	{
		QHash<QString, QString> states;
		for( const auto& program : std::as_const( m_programs ) )
		{
			const auto map = program.toMap();
			states[map.value( QStringLiteral("id") ).toString()] = map.value( QStringLiteral("state") ).toString();
		}
		m_programs.clear();
		const auto software = QJsonDocument::fromJson( message.argument( SoftwareDeployFeaturePlugin::Argument::SoftwareList ).toByteArray() ).array();
		for( const auto& value : software )
		{
			const auto program = value.toObject();
			const auto id = program[QStringLiteral("id")].toString();
			m_programs.append( QVariantMap{
				{ QStringLiteral("id"), id },
				{ QStringLiteral("name"), program[QStringLiteral("name")].toString() },
				{ QStringLiteral("version"), program[QStringLiteral("version")].toString() },
				{ QStringLiteral("removable"), program[QStringLiteral("removable")].toBool() || id.endsWith( QStringLiteral(".app") ) },
				{ QStringLiteral("state"), states.value( id ) },
			} );
		}
		Q_EMIT programsChanged();
		break;
	}
	case SoftwareDeployFeaturePlugin::Status:
	{
		const auto job = message.argument( SoftwareDeployFeaturePlugin::Argument::Job ).toString();
		const auto id = m_jobs.value( job );
		if( id.isEmpty() )
		{
			return;
		}
		const auto state = message.argument( SoftwareDeployFeaturePlugin::Argument::State ).toString();
		if( state == QLatin1String("done") )
		{
			setProgramState( id, QStringLiteral("removed") );
			m_jobs.remove( job );
		}
		else if( state == QLatin1String("failed") )
		{
			setProgramState( id, tr("gagal: %1").arg( message.argument( SoftwareDeployFeaturePlugin::Argument::Error ).toString() ) );
			m_jobs.remove( job );
		}
		break;
	}
	default:
		break;
	}
}



void ComputerInfoController::setState( const QString& state )
{
	if( state != m_state )
	{
		m_state = state;
		Q_EMIT stateChanged();
	}
}



void ComputerInfoController::setProgramState( const QString& id, const QString& state )
{
	for( auto& program : m_programs )
	{
		auto map = program.toMap();
		if( map.value( QStringLiteral("id") ).toString() == id )
		{
			map[QStringLiteral("state")] = state;
			program = map;
		}
	}
	Q_EMIT programsChanged();
}
