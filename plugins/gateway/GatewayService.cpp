/*
 * GatewayService.cpp - Aruni Gateway running inside veyon-server
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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QNetworkRequest>
#include <QSet>
#include <QUdpSocket>

#include "Filesystem.h"
#include "GatewayService.h"
#include "GatewaySession.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "PlatformFilesystemFunctions.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


namespace {

// the network discovery plugin also merges the manually configured computers
const auto NetworkDiscoveryPluginUid = Plugin::Uid( QStringLiteral("3c5e9a14-2b7d-4e6f-8a1c-9d0f2e4b6c81") );

}



GatewayService::GatewayService( QObject* parent ) :
	QObject( parent ),
	m_instanceLock( QDir( VeyonCore::platform().filesystemFunctions().globalTempPath() ).filePath( QStringLiteral("aruni-gateway.lock") ) )
{
	m_instanceLock.setStaleLockTime( 0 );

	connect( &m_control, &QWebSocket::connected, this, [this]() {
		m_connected = true;
		m_lastError.clear();
		m_reconnectDelay = 2000;
		vInfo() << "Aruni Gateway" << m_state.gatewayId << "connected to relay" << m_state.relayUrl;
		writeStatus();
	} );
	connect( &m_control, &QWebSocket::textMessageReceived, this, &GatewayService::onTextMessage );
	connect( &m_control, &QWebSocket::disconnected, this, &GatewayService::onDisconnected );
	connect( &m_control, &QWebSocket::errorOccurred, this, [this]( QAbstractSocket::SocketError ) {
		m_lastError = m_control.errorString();
	} );

	m_reconnectTimer.setSingleShot( true );
	connect( &m_reconnectTimer, &QTimer::timeout, this, &GatewayService::connectToRelay );

	connect( &m_stateTimer, &QTimer::timeout, this, &GatewayService::checkState );
	m_stateTimer.start( StateCheckInterval );

	connect( &m_directoryTimer, &QTimer::timeout, this, &GatewayService::updateDirectory );

	// proxies such as Cloudflare drop WebSockets idle for ~100 s - the relay
	// answers "ping" without further processing
	connect( &m_keepAliveTimer, &QTimer::timeout, this, [this]() {
		if( m_connected )
		{
			m_control.sendTextMessage( QStringLiteral("ping") );
		}
	} );
	m_keepAliveTimer.start( 30000 );

	// Masters on the office network ask here which roaming laptops they can
	// reach through this gateway (see NetworkDiscoveryDirectory)
	connect( &m_directoryServer, &QTcpServer::newConnection, this, [this]() {
		while( auto socket = m_directoryServer.nextPendingConnection() )
		{
			connect( socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater );
			if( isLocalNetworkAddress( socket->peerAddress() ) )
			{
				socket->write( roamingDirectoryJson() );
			}
			socket->disconnectFromHost();
		}
	} );

	checkState();
}



GatewayService::~GatewayService()
{
	const auto agents = m_onlineAgents.keys();
	for( const auto& key : agents )
	{
		dropAgent( key );
	}
	m_control.close();
	if( m_haveLock )
	{
		GatewayState::writeStatus( { { QStringLiteral("running"), false } } );
		m_instanceLock.unlock();
	}
}



bool GatewayService::authorizeDevice( const QByteArray& deviceKey, const QByteArray& pairingToken, const QString& deviceName )
{
	for( const auto& device : std::as_const( m_state.devices ) )
	{
		if( device.publicKey == deviceKey )
		{
			return true;
		}
	}

	if( pairingToken.isEmpty() || m_state.isPairingActive() == false || pairingToken != m_state.pairingToken )
	{
		return false;
	}

	// the pairing code is single use: register the device and invalidate it
	GatewayState::Device device;
	device.publicKey = deviceKey;
	device.name = deviceName.isEmpty() ? tr("Phone") : deviceName;
	device.added = QDateTime::currentDateTimeUtc();
	device.lastSeen = device.added;

	const bool saved = GatewayState::update( [&device]( GatewayState& state ) {
		state.devices.append( device );
		state.pairingToken.clear();
		state.pairingExpires = {};
	} );

	if( saved )
	{
		m_state.devices.append( device );
		m_state.pairingToken.clear();
		vInfo() << "Aruni Gateway: paired new device" << device.name;
	}

	return saved;
}



QString GatewayService::deviceName( const QByteArray& deviceKey ) const
{
	for( const auto& device : m_state.devices )
	{
		if( device.publicKey == deviceKey )
		{
			return device.name;
		}
	}
	return {};
}



void GatewayService::markDeviceSeen( const QByteArray& deviceKey )
{
	GatewayState::update( [&deviceKey]( GatewayState& state ) {
		for( auto& device : state.devices )
		{
			if( device.publicKey == deviceKey )
			{
				device.lastSeen = QDateTime::currentDateTimeUtc();
			}
		}
	} );
}



QByteArray GatewayService::gatewayInfo() const
{
	return QJsonDocument( QJsonObject{
		{ QStringLiteral("name"), m_state.siteName },
		{ QStringLiteral("id"), m_state.gatewayId },
		{ QStringLiteral("version"), VeyonCore::versionString() },
		{ QStringLiteral("port"), VeyonCore::config().veyonServerPort() },
		{ QStringLiteral("subnets"), QJsonArray::fromStringList( localSubnets() ) },
		// lets a roaming laptop find out whether it is in the office
		{ QStringLiteral("addresses"), QJsonArray::fromStringList( localAddresses() ) },
		{ QStringLiteral("keyAvailable"), sharedKeyJson().isEmpty() == false },
	} ).toJson( QJsonDocument::Compact );
}



QByteArray GatewayService::sharedKeyJson() const
{
	if( m_state.sharedKeyName.isEmpty() || VeyonCore::isAuthenticationKeyNameValid( m_state.sharedKeyName ) == false )
	{
		return {};
	}

	QFile file( VeyonCore::filesystem().privateKeyPath( m_state.sharedKeyName ) );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}

	return QJsonDocument( QJsonObject{
		{ QStringLiteral("name"), m_state.sharedKeyName },
		{ QStringLiteral("pem"), QString::fromUtf8( file.readAll() ) },
	} ).toJson( QJsonDocument::Compact );
}



QByteArray GatewayService::hostsJson() const
{
	QJsonArray hosts;
	QSet<QString> seen;

	if( m_directory )
	{
		const auto objects = m_directory->queryObjects( NetworkObject::Type::Host, NetworkObject::Attribute::None, {} );
		for( const auto& object : objects )
		{
			const auto host = object.hostAddress().isEmpty() ? object.name() : object.hostAddress();
			if( host.isEmpty() || seen.contains( host ) || isRoamingForward( host ) )
			{
				continue;
			}
			seen.insert( host );

			QString location;
			const auto parents = m_directory->queryParents( object );
			for( const auto& parent : parents )
			{
				if( parent.type() == NetworkObject::Type::Location )
				{
					location = parent.name();
					break;
				}
			}

			hosts.append( QJsonObject{
				{ QStringLiteral("name"), object.name() },
				{ QStringLiteral("host"), host },
				{ QStringLiteral("mac"), object.macAddress() },
				{ QStringLiteral("location"), location },
			} );
		}
	}

	for( auto it = m_onlineAgents.cbegin(); it != m_onlineAgents.cend(); ++it )
	{
		const auto agent = findAgent( it.key() );
		if( agent == nullptr || isAgentLocal( it.value() ) )
		{
			// in the office the laptop is already listed with its LAN address
			continue;
		}

		hosts.append( QJsonObject{
			{ QStringLiteral("name"), agent->name },
			{ QStringLiteral("host"), agent->id() },
			{ QStringLiteral("roaming"), true },
			{ QStringLiteral("user"), it->hello[QStringLiteral("user")].toString() },
		} );
	}

	return QJsonDocument( hosts ).toJson( QJsonDocument::Compact );
}



bool GatewayService::authorizeAgent( const QByteArray& agentKey, const QByteArray& enrollmentToken, const QString& name )
{
	if( findAgent( agentKey ) )
	{
		return true;
	}

	if( enrollmentToken.size() != AruniTunnel::TokenSize || enrollmentToken != m_state.enrollmentToken )
	{
		return false;
	}

	GatewayState::Agent agent;
	agent.publicKey = agentKey;
	agent.name = name.isEmpty() ? tr("Laptop") : name;
	agent.added = QDateTime::currentDateTimeUtc();
	agent.lastSeen = agent.added;

	bool registered = false;
	const bool saved = GatewayState::update( [&agent, &registered]( GatewayState& state ) {
		agent.slot = state.freeAgentSlot();
		if( agent.slot > 0 )
		{
			state.agents.append( agent );
			registered = true;
		}
	} );

	if( saved == false || registered == false )
	{
		vWarning() << "Aruni Gateway: cannot register roaming laptop" << agent.name << "- no free slot";
		return false;
	}

	m_state.agents.append( agent );
	vInfo() << "Aruni Gateway: registered roaming laptop" << agent.name << "on port" << GatewayState::agentPort( agent.slot );
	return true;
}



QString GatewayService::agentName( const QByteArray& agentKey ) const
{
	const auto agent = findAgent( agentKey );
	return agent ? agent->name : QString{};
}



void GatewayService::agentConnected( GatewaySession* session )
{
	const auto key = session->peerKey();
	const auto agent = findAgent( key );
	if( agent == nullptr )
	{
		session->finish();
		return;
	}

	// a laptop that reconnected before the old session timed out
	dropAgent( key );

	auto server = new QTcpServer( this );
	const auto port = GatewayState::agentPort( agent->slot );
	if( server->listen( QHostAddress::Any, port ) == false )
	{
		vWarning() << "Aruni Gateway: cannot listen on port" << port << "for" << agent->name << server->errorString();
		delete server;
		session->finish();
		return;
	}

	QPointer<GatewaySession> sessionPointer( session );
	connect( server, &QTcpServer::newConnection, this, [server, sessionPointer]() {
		while( auto socket = server->nextPendingConnection() )
		{
			// Masters of the office network only - never the internet
			if( sessionPointer && isLocalNetworkAddress( socket->peerAddress() ) )
			{
				sessionPointer->forwardToAgent( socket );
			}
			else
			{
				socket->abort();
				socket->deleteLater();
			}
		}
	} );

	OnlineAgent online;
	online.session = session;
	online.server = server;
	online.since = QDateTime::currentDateTimeUtc();
	m_onlineAgents.insert( key, online );

	writeStatus();
}



void GatewayService::agentDisconnected( GatewaySession* session )
{
	const auto key = session->peerKey();
	const auto it = m_onlineAgents.find( key );
	if( it == m_onlineAgents.end() || it->session != session )
	{
		return;
	}

	delete it->server;
	m_onlineAgents.erase( it );

	GatewayState::update( [&key]( GatewayState& state ) {
		for( auto& agent : state.agents )
		{
			if( agent.publicKey == key )
			{
				agent.lastSeen = QDateTime::currentDateTimeUtc();
			}
		}
	} );

	writeStatus();
}



void GatewayService::agentHello( const QByteArray& agentKey, const QJsonObject& hello )
{
	const auto it = m_onlineAgents.find( agentKey );
	if( it != m_onlineAgents.end() )
	{
		it->hello = hello;
		writeStatus();
	}
}



quint16 GatewayService::agentPort( const QString& host ) const
{
	for( auto it = m_onlineAgents.cbegin(); it != m_onlineAgents.cend(); ++it )
	{
		const auto agent = findAgent( it.key() );
		if( agent && agent->id().compare( host, Qt::CaseInsensitive ) == 0 )
		{
			return GatewayState::agentPort( agent->slot );
		}
	}
	return 0;
}



bool GatewayService::isRoamingForward( const QString& host )
{
	// "<gateway>:<port>" entries of roaming laptops (found through the
	// directory service of this or another gateway) are listed as roaming
	// laptops instead
	const auto separator = host.lastIndexOf( QLatin1Char(':') );
	if( separator < 0 || host.count( QLatin1Char(':') ) > 1 )
	{
		return false;
	}
	const auto port = host.mid( separator + 1 ).toInt();
	return port >= GatewayState::agentPort( 1 ) && port <= GatewayState::agentPort( GatewayState::MaxAgentSlot );
}



const GatewayState::Agent* GatewayService::findAgent( const QByteArray& agentKey ) const
{
	for( const auto& agent : m_state.agents )
	{
		if( agent.publicKey == agentKey )
		{
			return &agent;
		}
	}
	return nullptr;
}



bool GatewayService::isAgentLocal( const OnlineAgent& agent ) const
{
	// the laptop reports whether it reached this gateway's directory service on
	// the local network (see RoamingAgent::probeOffice())
	return agent.hello[QStringLiteral("local")].toBool();
}



void GatewayService::dropAgent( const QByteArray& agentKey )
{
	const auto it = m_onlineAgents.find( agentKey );
	if( it == m_onlineAgents.end() )
	{
		return;
	}

	delete it->server;
	const auto session = it->session;
	m_onlineAgents.erase( it );
	if( session )
	{
		session->finish();
	}
}



void GatewayService::dropRemovedAgents()
{
	const auto keys = m_onlineAgents.keys();
	for( const auto& key : keys )
	{
		if( findAgent( key ) == nullptr )
		{
			vInfo() << "Aruni Gateway: disconnecting removed roaming laptop";
			dropAgent( key );
		}
	}
}



void GatewayService::updateDirectoryServer()
{
	const bool wanted = m_haveLock && m_state.enabled;
	if( wanted && m_directoryServer.isListening() == false )
	{
		const auto port = GatewayState::directoryPort();
		if( m_directoryServer.listen( QHostAddress::Any, port ) == false )
		{
			vWarning() << "Aruni Gateway: cannot listen on directory port" << port << m_directoryServer.errorString();
		}
	}
	else if( wanted == false && m_directoryServer.isListening() )
	{
		m_directoryServer.close();
	}
}



QByteArray GatewayService::roamingDirectoryJson() const
{
	QJsonArray laptops;
	for( auto it = m_onlineAgents.cbegin(); it != m_onlineAgents.cend(); ++it )
	{
		const auto agent = findAgent( it.key() );
		if( agent && isAgentLocal( it.value() ) == false )
		{
			laptops.append( QJsonObject{
				{ QStringLiteral("name"), agent->name },
				{ QStringLiteral("port"), GatewayState::agentPort( agent->slot ) },
				{ QStringLiteral("id"), agent->id() },
			} );
		}
	}

	return QJsonDocument( QJsonObject{
		{ QStringLiteral("id"), m_state.gatewayId },
		{ QStringLiteral("site"), m_state.siteName },
		{ QStringLiteral("laptops"), laptops },
	} ).toJson( QJsonDocument::Compact );
}



void GatewayService::wakeOnLan( const QString& macAddress )
{
	auto mac = QByteArray::fromHex( macAddress.toLatin1().replace( ':', "" ).replace( '-', "" ) );
	if( mac.size() != 6 )
	{
		return;
	}

	QByteArray packet( 6, char( 0xff ) );
	for( int i = 0; i < 16; ++i )
	{
		packet.append( mac );
	}

	QUdpSocket socket;
	socket.writeDatagram( packet, QHostAddress::Broadcast, 9 );
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		for( const auto& entry : iface.addressEntries() )
		{
			if( entry.broadcast().isNull() == false )
			{
				socket.writeDatagram( packet, entry.broadcast(), 9 );
			}
		}
	}
	vInfo() << "Aruni Gateway: sent Wake-on-LAN packet to" << macAddress;
}



void GatewayService::checkState()
{
	const auto modified = QFileInfo( GatewayState::statePath() ).lastModified();
	const bool changed = modified != m_stateModified;
	if( changed )
	{
		const auto previous = m_state;
		m_state = GatewayState::load();
		m_stateModified = modified;

		const bool identityChanged = previous.gatewayId != m_state.gatewayId || previous.relayUrl != m_state.relayUrl ||
									 previous.relaySecret != m_state.relaySecret;
		if( identityChanged && m_control.state() != QAbstractSocket::UnconnectedState )
		{
			m_control.close();
		}

		dropRemovedAgents();
	}

	if( m_state.enabled == false )
	{
		if( m_haveLock )
		{
			const auto agents = m_onlineAgents.keys();
			for( const auto& key : agents )
			{
				dropAgent( key );
			}
			m_control.close();
			m_reconnectTimer.stop();
			m_directoryTimer.stop();
			updateDirectoryServer();
			delete m_directory;
			m_directory = nullptr;
			GatewayState::writeStatus( { { QStringLiteral("running"), false }, { QStringLiteral("enabled"), false } } );
			m_instanceLock.unlock();
			m_haveLock = false;
		}
		return;
	}

	// only one server instance per computer acts as gateway
	if( m_haveLock == false )
	{
		m_haveLock = m_instanceLock.tryLock( 0 );
		if( m_haveLock == false )
		{
			return;
		}

		// persist a freshly created identity so the Configurator shows the same one
		GatewayState::update( []( GatewayState& ) {} );
		m_state = GatewayState::load();
		m_stateModified = QFileInfo( GatewayState::statePath() ).lastModified();

		m_directory = VeyonCore::networkObjectDirectoryManager().createDirectory( NetworkDiscoveryPluginUid, this );
		updateDirectory();
		m_directoryTimer.start( DirectoryUpdateInterval );
	}

	if( m_control.state() == QAbstractSocket::UnconnectedState && m_reconnectTimer.isActive() == false )
	{
		connectToRelay();
	}

	updateDirectoryServer();
	writeStatus();
}



void GatewayService::connectToRelay()
{
	if( m_haveLock == false || m_state.enabled == false )
	{
		return;
	}

	QUrl url( m_state.relayUrl );
	url.setPath( url.path() + QStringLiteral("/v1/gateway/") + m_state.gatewayId );

	QNetworkRequest request( url );
	request.setRawHeader( "X-Aruni-Secret", m_state.relaySecret.toUtf8() );
	m_control.open( request );
}



void GatewayService::onTextMessage( const QString& message )
{
	const auto json = QJsonDocument::fromJson( message.toUtf8() ).object();
	if( json[QStringLiteral("type")].toString() != QStringLiteral("session") )
	{
		return;
	}

	const auto sessionId = json[QStringLiteral("sid")].toString();
	if( sessionId.isEmpty() || sessionId.size() > 64 )
	{
		return;
	}

	QUrl url( m_state.relayUrl );
	url.setPath( url.path() + QStringLiteral("/v1/accept/%1/%2").arg( m_state.gatewayId, sessionId ) );

	auto session = new GatewaySession( this, sessionId );
	++m_sessions;
	connect( session, &GatewaySession::finished, this, [this]() {
		--m_sessions;
		writeStatus();
	} );
	session->start( url, m_state.relaySecret.toUtf8() );

	updateDirectory();
	writeStatus();
}



void GatewayService::onDisconnected()
{
	const bool wasConnected = m_connected;
	m_connected = false;

	if( m_lastError.isEmpty() && m_control.closeReason().isEmpty() == false )
	{
		m_lastError = m_control.closeReason();
	}

	if( wasConnected )
	{
		vWarning() << "Aruni Gateway lost the relay connection:" << m_lastError;
	}

	writeStatus();

	if( m_haveLock && m_state.enabled )
	{
		m_reconnectTimer.start( m_reconnectDelay );
		m_reconnectDelay = qMin( m_reconnectDelay * 2, MaxReconnectDelay );
	}
}



void GatewayService::updateDirectory()
{
	if( m_directory )
	{
		m_directory->update();
	}
}



void GatewayService::writeStatus()
{
	if( m_haveLock == false )
	{
		return;
	}

	QJsonArray agents;
	for( auto it = m_onlineAgents.cbegin(); it != m_onlineAgents.cend(); ++it )
	{
		agents.append( QJsonObject{
			{ QStringLiteral("key"), AruniTunnel::toBase64Url( it.key() ) },
			{ QStringLiteral("local"), isAgentLocal( it.value() ) },
			{ QStringLiteral("since"), it->since.toString( Qt::ISODate ) },
			{ QStringLiteral("user"), it->hello[QStringLiteral("user")].toString() },
			{ QStringLiteral("addresses"), it->hello[QStringLiteral("addresses")].toArray() },
		} );
	}

	GatewayState::writeStatus( {
		{ QStringLiteral("agents"), agents },
		{ QStringLiteral("running"), true },
		{ QStringLiteral("enabled"), m_state.enabled },
		{ QStringLiteral("connected"), m_connected },
		{ QStringLiteral("error"), m_lastError },
		{ QStringLiteral("sessions"), m_sessions },
		{ QStringLiteral("id"), m_state.gatewayId },
		{ QStringLiteral("relay"), m_state.relayUrl },
		{ QStringLiteral("updated"), QDateTime::currentDateTimeUtc().toString( Qt::ISODate ) },
	} );
}



bool GatewayService::isLocalNetworkAddress( const QHostAddress& address )
{
	if( address.isLoopback() )
	{
		return true;
	}

	// IPv4-mapped IPv6 addresses (dual-stack sockets)
	bool isIPv4 = false;
	const QHostAddress ipv4( address.toIPv4Address( &isIPv4 ) );
	if( isIPv4 )
	{
		if( ipv4.isLoopback() )
		{
			return true;
		}
		for( const auto& subnet : { QStringLiteral("10.0.0.0/8"), QStringLiteral("172.16.0.0/12"),
									QStringLiteral("192.168.0.0/16"), QStringLiteral("100.64.0.0/10"),
									QStringLiteral("169.254.0.0/16") } )
		{
			if( ipv4.isInSubnet( QHostAddress::parseSubnet( subnet ) ) )
			{
				return true;
			}
		}
		return false;
	}

	return address.isInSubnet( QHostAddress::parseSubnet( QStringLiteral("fc00::/7") ) ) ||
		   address.isLinkLocal();
}



QStringList GatewayService::localAddresses()
{
	QStringList addresses;
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		if( iface.flags().testFlag( QNetworkInterface::IsUp ) == false ||
			iface.flags().testFlag( QNetworkInterface::IsLoopBack ) )
		{
			continue;
		}
		for( const auto& entry : iface.addressEntries() )
		{
			if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol )
			{
				addresses.append( entry.ip().toString() );
			}
		}
	}
	return addresses;
}



QStringList GatewayService::localSubnets() const
{
	QStringList subnets;
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		if( iface.flags().testFlag( QNetworkInterface::IsUp ) == false ||
			iface.flags().testFlag( QNetworkInterface::IsLoopBack ) )
		{
			continue;
		}
		for( const auto& entry : iface.addressEntries() )
		{
			if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol && entry.prefixLength() >= 16 )
			{
				const auto network = QHostAddress( entry.ip().toIPv4Address() & entry.netmask().toIPv4Address() );
				subnets.append( QStringLiteral("%1/%2").arg( network.toString() ).arg( entry.prefixLength() ) );
			}
		}
	}
	subnets.removeDuplicates();
	return subnets;
}
