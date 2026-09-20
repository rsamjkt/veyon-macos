/*
 * NetworkDiscoveryDirectory.cpp - implementation of NetworkDiscoveryDirectory
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

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QTimer>

#include "NetworkDiscoveryDirectory.h"
#include "VeyonConfiguration.h"


NetworkDiscoveryDirectory::NetworkDiscoveryDirectory( QObject* parent ) :
	NetworkObjectDirectory( parent ),
	m_location( NetworkObject::Type::Location,
				tr( "Discovered computers" ), {}, {}, {},
				NetworkObject::Uid( QStringLiteral("{b0d9a7e2-1c34-4f55-9a6b-7c8d9e0f1a23}") ) ),
	m_serverPort( VeyonCore::config().veyonServerPort() )
{
	m_scanTimeout = new QTimer( this );
	m_scanTimeout->setSingleShot( true );
	connect( m_scanTimeout, &QTimer::timeout, this, &NetworkDiscoveryDirectory::finishScan );
}



void NetworkDiscoveryDirectory::update()
{
	addOrUpdateObject( m_location, rootObject() );

	// Scanning only covers subnets small enough to sweep (see scanTargets()), so
	// hosts on e.g. a /16 VPN overlay can never be discovered. Show the manually
	// configured objects alongside the discovered ones so those hosts are still
	// reachable from the master without having to switch directory backends.
	updateConfiguredObjects();

	if( m_scanning )
	{
		return;
	}

	startScan();
}



void NetworkDiscoveryDirectory::updateConfiguredObjects()
{
	const auto networkObjects = VeyonCore::config().value( QStringLiteral("NetworkObjects"),
														   QStringLiteral("BuiltinDirectory"),
														   {} ).toJsonArray();

	vDebug() << "NetworkDiscovery: configured objects in store:" << networkObjects.size();

	NetworkObjectUidList locationUids;

	for( const auto& networkObjectValue : networkObjects )
	{
		const NetworkObject networkObject( networkObjectValue.toObject() );

		if( networkObject.type() == NetworkObject::Type::Location )
		{
			locationUids.append( networkObject.uid() ); // clazy:exclude=reserve-candidates
			addOrUpdateObject( networkObject, rootObject() );
			updateConfiguredLocation( networkObject, networkObjects );
		}
	}

	// drop configured locations that have been removed, but never the location
	// holding the discovered computers
	const auto discoveredUid = m_location.uid();
	removeObjects( rootObject(), [locationUids, discoveredUid]( const NetworkObject& object ) {
		return object.type() == NetworkObject::Type::Location &&
			   object.uid() != discoveredUid &&
			   locationUids.contains( object.uid() ) == false; } );
}



void NetworkDiscoveryDirectory::updateConfiguredLocation( const NetworkObject& locationObject,
														  const QJsonArray& networkObjects )
{
	NetworkObjectUidList computerUids;

	for( const auto& networkObjectValue : networkObjects )
	{
		const NetworkObject networkObject( networkObjectValue.toObject() );

		if( networkObject.parentUid() == locationObject.uid() )
		{
			computerUids.append( networkObject.uid() ); // clazy:exclude=reserve-candidates
			addOrUpdateObject( networkObject, locationObject );
		}
	}

	vDebug() << "NetworkDiscovery: location" << locationObject.name() << "has" << computerUids.size() << "computer(s)";

	setObjectPopulated( locationObject );

	removeObjects( locationObject, [computerUids]( const NetworkObject& object ) {
		return object.type() == NetworkObject::Type::Host &&
			   computerUids.contains( object.uid() ) == false; } );
}



void NetworkDiscoveryDirectory::startScan()
{
	m_scanning = true;
	m_foundHosts.clear();

	const auto targets = scanTargets();
	vDebug() << "NetworkDiscovery: scanning" << targets.size() << "hosts on port" << m_serverPort;
	if( targets.isEmpty() )
	{
		finishScan();
		return;
	}

	for( const auto& address : targets )
	{
		auto socket = new QTcpSocket( this );
		const auto hostString = address.toString();

		connect( socket, &QTcpSocket::connected, this, [this, socket, hostString]() {
			m_foundHosts.insert( hostString );
			socket->abort();
		} );
		connect( socket, &QTcpSocket::errorOccurred, socket, [socket]( QAbstractSocket::SocketError ) {
			socket->abort();
		} );

		socket->connectToHost( address, static_cast<quint16>( m_serverPort ) );
		m_pendingSockets.append( socket );
	}

	m_scanTimeout->start( ScanTimeoutMs );
}



void NetworkDiscoveryDirectory::finishScan()
{
	for( auto socket : std::as_const( m_pendingSockets ) )
	{
		socket->abort();
		socket->deleteLater();
	}
	m_pendingSockets.clear();

	// age out hosts that keep missing scans, but keep recently seen ones so a
	// single slow reply does not tear down an active connection
	for( auto it = m_knownHosts.begin(); it != m_knownHosts.end(); )
	{
		if( m_foundHosts.contains( it.key() ) )
		{
			it.value() = 0;
			++it;
		}
		else if( ++it.value() >= MaxMissedScans )
		{
			it = m_knownHosts.erase( it );
		}
		else
		{
			++it;
		}
	}

	for( const auto& host : std::as_const( m_foundHosts ) )
	{
		m_knownHosts.insert( host, 0 );
	}

	NetworkObjectList computers;
	computers.reserve( m_knownHosts.size() );
	for( const auto& host : m_knownHosts.keys() )
	{
		const auto uid = NetworkObject::Uid::createUuidV5( NetworkObject::Uid(),
														   QStringLiteral("aruni-discovery:") + host );
		computers.append( NetworkObject( NetworkObject::Type::Host, host, host, {}, {}, uid ) );
	}

	replaceObjects( computers, m_location );
	setObjectPopulated( m_location );
	propagateChildObjectChanges();

	vDebug() << "NetworkDiscovery: scan finished, found" << m_foundHosts.size()
			 << "AruniControl server(s):" << QStringList( m_foundHosts.begin(), m_foundHosts.end() )
			 << "- listing" << m_knownHosts.size() << "host(s) including recently seen ones";

	m_scanning = false;
}



QList<QHostAddress> NetworkDiscoveryDirectory::scanTargets() const
{
	QList<QHostAddress> targets;

	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		const auto flags = iface.flags();
		if( flags.testFlag( QNetworkInterface::IsUp ) == false ||
			flags.testFlag( QNetworkInterface::IsLoopBack ) ||
			flags.testFlag( QNetworkInterface::IsRunning ) == false )
		{
			continue;
		}

		const auto entries = iface.addressEntries();
		for( const auto& entry : entries )
		{
			const auto ip = entry.ip();
			if( ip.protocol() != QAbstractSocket::IPv4Protocol )
			{
				continue;
			}

			const int prefix = entry.prefixLength();
			// only scan reasonably sized subnets (>= /20, i.e. up to 4094 hosts)
			if( prefix < 20 || prefix > 31 )
			{
				continue;
			}

			const quint32 ipv4 = ip.toIPv4Address();
			const quint32 mask = ( prefix == 0 ) ? 0u : ( ~0u << ( 32 - prefix ) );
			const quint32 network = ipv4 & mask;
			const quint32 broadcast = network | ~mask;

			for( quint32 host = network + 1; host < broadcast; ++host )
			{
				targets.append( QHostAddress( host ) );
				if( targets.size() >= MaxHostsPerScan )
				{
					return targets;
				}
			}
		}
	}

	return targets;
}
