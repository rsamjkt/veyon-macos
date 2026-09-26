/*
 * GatewayManager.cpp - connects the mobile Master to Aruni Gateways via the relay
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

#include <QDateTime>
#include <QDir>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QtEndian>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif

#include "GatewayManager.h"
#include "PlatformUserFunctions.h"
#include "VeyonCore.h"


GatewayManager* GatewayManager::s_instance = nullptr;

namespace {

QSettings gatewaySettings()
{
	const auto path = QStandardPaths::writableLocation( QStandardPaths::AppDataLocation );
	QDir().mkpath( path );
	return QSettings{ QDir( path ).filePath( QStringLiteral("mobile.ini") ), QSettings::IniFormat };
}

QList<QPair<QHostAddress, int>> parseSubnets( const QStringList& subnets )
{
	QList<QPair<QHostAddress, int>> parsed;
	for( const auto& subnet : subnets )
	{
		const auto entry = QHostAddress::parseSubnet( subnet );
		if( entry.first.isNull() == false )
		{
			parsed.append( entry );
		}
	}
	return parsed;
}

#ifdef Q_OS_ANDROID
void JNICALL nativeQrScanned( JNIEnv*, jclass, jstring code, jstring error )
{
	const auto codeText = code ? QJniObject( code ).toString() : QString{};
	const auto errorText = error ? QJniObject( error ).toString() : QString{};
	if( auto manager = GatewayManager::instance() )
	{
		QMetaObject::invokeMethod( manager, [manager, codeText, errorText]() {
			manager->handleScannedCode( codeText, errorText );
		}, Qt::QueuedConnection );
	}
}
#endif

}


// ============================================================================ GatewayLink

GatewayLink::GatewayLink( GatewayManager* manager, const GatewaySite& site, const AruniTunnel::KeyPair& device ) :
	QObject( manager ),
	m_manager( manager ),
	m_site( site ),
	m_device( device )
{
	connect( &m_socket, &QWebSocket::connected, this, &GatewayLink::onConnected );
	connect( &m_socket, &QWebSocket::binaryMessageReceived, this, &GatewayLink::onBinaryMessage );
	connect( &m_socket, &QWebSocket::disconnected, this, &GatewayLink::onDisconnected );
	connect( &m_socket, &QWebSocket::errorOccurred, this, [this]( QAbstractSocket::SocketError ) {
		m_error = m_socket.errorString();
	} );
	connect( &m_socket, &QWebSocket::bytesWritten, this, [this]( qint64 bytes ) {
		m_pendingBytes = qMax<qint64>( 0, m_pendingBytes - bytes );
		if( m_pendingBytes < MaxPendingBytes / 2 )
		{
			const auto streams = m_streams.keys();
			for( const auto stream : streams )
			{
				forwardLocalData( stream );
			}
		}
	} );

	m_pingTimer.setInterval( 20000 );
	connect( &m_pingTimer, &QTimer::timeout, this, [this]() {
		if( QDateTime::currentMSecsSinceEpoch() - m_lastPong > 60000 )
		{
			vWarning() << "gateway" << m_site.gatewayId << "stopped answering";
			m_socket.abort();
			return;
		}
		sendFrame( AruniTunnel::FrameType::Ping, 0 );
	} );

	m_hostsTimer.setInterval( 60000 );
	connect( &m_hostsTimer, &QTimer::timeout, this, [this]() {
		sendFrame( AruniTunnel::FrameType::HostsRequest, 0 );
	} );

	m_reconnectTimer.setSingleShot( true );
	connect( &m_reconnectTimer, &QTimer::timeout, this, &GatewayLink::start );
}



GatewayLink::~GatewayLink()
{
	m_running = false;
	closeListeners();
	m_socket.abort();
}



bool GatewayLink::isOnSite() const
{
	const auto subnets = parseSubnets( m_subnets );
	const auto interfaces = QNetworkInterface::allAddresses();
	for( const auto& address : interfaces )
	{
		if( address.isLoopback() || address.protocol() != QAbstractSocket::IPv4Protocol )
		{
			continue;
		}
		for( const auto& subnet : subnets )
		{
			if( address.isInSubnet( subnet ) )
			{
				return true;
			}
		}
	}
	return false;
}



bool GatewayLink::covers( const QString& host ) const
{
	for( const auto& value : m_hosts )
	{
		if( value.toObject()[QStringLiteral("host")].toString().compare( host, Qt::CaseInsensitive ) == 0 )
		{
			return true;
		}
	}

	const QHostAddress address( host );
	if( address.isNull() == false )
	{
		for( const auto& subnet : parseSubnets( m_subnets ) )
		{
			if( address.isInSubnet( subnet ) )
			{
				return true;
			}
		}
	}

	return false;
}



void GatewayLink::start()
{
	m_running = true;
	m_established = false;

	QUrl url( m_site.relayUrl );
	url.setPath( url.path() + QStringLiteral("/v1/connect/") + m_site.gatewayId );

	setState( QStringLiteral("connecting") );
	m_socket.open( url );
}



void GatewayLink::stop()
{
	m_running = false;
	m_reconnectTimer.stop();
	m_socket.close();
	closeListeners();
	setState( QStringLiteral("offline") );
}



int GatewayLink::localPortFor( const QString& host, int port )
{
	if( isOnline() == false )
	{
		return -1;
	}

	const auto key = QStringLiteral("%1:%2").arg( host ).arg( port );
	if( auto server = m_listeners.value( key ) )
	{
		return server->serverPort();
	}

	auto server = new QTcpServer( this );
	if( server->listen( QHostAddress::LocalHost, 0 ) == false )
	{
		delete server;
		return -1;
	}

	connect( server, &QTcpServer::newConnection, this, [this, server, host, port]() {
		acceptLocalConnection( server, host, port );
	} );
	m_listeners.insert( key, server );
	return server->serverPort();
}



void GatewayLink::wake( const QString& macAddress )
{
	sendFrame( AruniTunnel::FrameType::Wake, 0, macAddress.toUtf8() );
}



void GatewayLink::setState( const QString& state, const QString& error )
{
	if( state != m_state || error != m_error )
	{
		m_state = state;
		m_error = error;
		Q_EMIT stateChanged();
	}
}



void GatewayLink::onConnected()
{
	m_pairing = m_site.pairingToken.size() == AruniTunnel::TokenSize;
	const auto deviceName = VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::FullName );

	m_handshake = std::make_unique<AruniTunnel::MasterHandshake>( m_device, m_site.publicKey,
																 m_pairing ? m_site.pairingToken : QByteArray{},
																 deviceName.isEmpty() ? QStringLiteral("AruniControl") : deviceName );
	m_socket.sendBinaryMessage( m_handshake->firstMessage() );
}



void GatewayLink::onBinaryMessage( const QByteArray& message )
{
	if( m_established == false )
	{
		QByteArray info;
		if( m_handshake == nullptr || m_handshake->processReply( message, m_channel, info ) == false )
		{
			setState( QStringLiteral("error"), tr("Gateway menolak HP ini. Pindai ulang kode QR dari Configurator.") );
			m_running = false;
			m_socket.close();
			return;
		}

		m_handshake.reset();
		m_established = true;
		m_lastPong = QDateTime::currentMSecsSinceEpoch();
		m_reconnectDelay = 2000;

		const auto json = QJsonDocument::fromJson( info ).object();
		m_subnets.clear();
		for( const auto& value : json[QStringLiteral("subnets")].toArray() )
		{
			m_subnets.append( value.toString() );
		}
		if( json[QStringLiteral("name")].toString().isEmpty() == false )
		{
			m_site.name = json[QStringLiteral("name")].toString();
		}

		if( m_pairing )
		{
			m_site.pairingToken.clear();
			Q_EMIT paired();
			if( json[QStringLiteral("keyAvailable")].toBool() )
			{
				sendFrame( AruniTunnel::FrameType::KeyRequest, 0 );
			}
		}

		setState( QStringLiteral("online") );
		sendFrame( AruniTunnel::FrameType::HostsRequest, 0 );
		m_pingTimer.start();
		m_hostsTimer.start();
		return;
	}

	QByteArray frame;
	if( m_channel.decrypt( message, frame ) == false )
	{
		vWarning() << "invalid message from gateway" << m_site.gatewayId;
		m_socket.abort();
		return;
	}

	AruniTunnel::FrameType type;
	quint32 stream = 0;
	QByteArray payload;
	if( AruniTunnel::parseFrame( frame, type, stream, payload ) )
	{
		handleFrame( type, stream, payload );
	}
}



void GatewayLink::onDisconnected()
{
	const auto reason = m_socket.closeReason();
	const auto code = m_socket.closeCode();

	m_established = false;
	m_pingTimer.stop();
	m_hostsTimer.stop();
	closeListeners();

	if( m_state == QStringLiteral("error") )
	{
		return;
	}

	QString error;
	if( int( code ) == 4404 )
	{
		error = tr("Gateway sedang offline (PC gateway mati atau tidak terhubung internet).");
	}
	else if( reason.isEmpty() == false )
	{
		error = reason;
	}
	else if( m_error.isEmpty() == false )
	{
		error = m_error;
	}

	setState( QStringLiteral("offline"), error );
	scheduleReconnect();
}



void GatewayLink::handleFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload )
{
	using AruniTunnel::FrameType;

	switch( type )
	{
	case FrameType::Opened:
		if( m_streams.contains( stream ) )
		{
			m_streams[stream].opened = true;
			forwardLocalData( stream );
		}
		break;
	case FrameType::Data:
		if( auto socket = m_streams.value( stream ).socket )
		{
			socket->write( payload );
		}
		break;
	case FrameType::Close:
		closeStream( stream, false );
		break;
	case FrameType::Hosts:
	{
		const auto hosts = QJsonDocument::fromJson( payload ).array();
		if( hosts != m_hosts )
		{
			m_hosts = hosts;
			Q_EMIT hostsChanged();
		}
		break;
	}
	case FrameType::Key:
	{
		const auto json = QJsonDocument::fromJson( payload ).object();
		const auto name = json[QStringLiteral("name")].toString();
		const auto pem = json[QStringLiteral("pem")].toString();
		if( name.isEmpty() == false && pem.isEmpty() == false )
		{
			Q_EMIT accessKeyReceived( name, pem );
		}
		break;
	}
	case FrameType::Pong:
		m_lastPong = QDateTime::currentMSecsSinceEpoch();
		break;
	default:
		break;
	}
}



void GatewayLink::acceptLocalConnection( QTcpServer* server, const QString& host, int port )
{
	while( auto socket = server->nextPendingConnection() )
	{
		if( isOnline() == false )
		{
			socket->abort();
			socket->deleteLater();
			continue;
		}

		const auto stream = m_nextStream;
		m_nextStream += 2;

		socket->setParent( this );
		socket->setSocketOption( QAbstractSocket::LowDelayOption, 1 );
		m_streams.insert( stream, Stream{ socket, false } );

		connect( socket, &QTcpSocket::readyRead, this, [this, stream]() { forwardLocalData( stream ); } );
		connect( socket, &QTcpSocket::disconnected, this, [this, stream]() { closeStream( stream, true ); } );

		QByteArray payload( 2, 0 );
		qToBigEndian<quint16>( quint16( port ), reinterpret_cast<uchar*>( payload.data() ) );
		payload.append( host.toUtf8() );
		sendFrame( AruniTunnel::FrameType::Open, stream, payload );
	}
}



void GatewayLink::forwardLocalData( quint32 stream )
{
	const auto entry = m_streams.value( stream );
	if( entry.socket == nullptr || entry.opened == false )
	{
		// keep data in the socket until the gateway connected the target
		return;
	}

	while( entry.socket->bytesAvailable() > 0 && m_pendingBytes < MaxPendingBytes )
	{
		sendFrame( AruniTunnel::FrameType::Data, stream, entry.socket->read( ReadChunkSize ) );
	}
}



void GatewayLink::closeStream( quint32 stream, bool notify )
{
	if( m_streams.contains( stream ) == false )
	{
		return;
	}

	const auto entry = m_streams.take( stream );
	if( entry.socket )
	{
		entry.socket->disconnect( this );
		entry.socket->disconnectFromHost();
		entry.socket->deleteLater();
	}

	if( notify )
	{
		sendFrame( AruniTunnel::FrameType::Close, stream );
	}
}



void GatewayLink::sendFrame( AruniTunnel::FrameType type, quint32 stream, const QByteArray& payload )
{
	if( m_established == false )
	{
		return;
	}

	const auto message = m_channel.encrypt( AruniTunnel::makeFrame( type, stream, payload ) );
	m_pendingBytes += message.size();
	m_socket.sendBinaryMessage( message );
}



void GatewayLink::closeListeners()
{
	const auto streams = m_streams.keys();
	for( const auto stream : streams )
	{
		closeStream( stream, false );
	}

	for( auto server : std::as_const( m_listeners ) )
	{
		server->close();
		server->deleteLater();
	}
	m_listeners.clear();
}



void GatewayLink::scheduleReconnect()
{
	if( m_running )
	{
		m_reconnectTimer.start( m_reconnectDelay );
		m_reconnectDelay = qMin( m_reconnectDelay * 2, 30000 );
	}
}


// ============================================================================ GatewayManager

GatewayManager::GatewayManager( QObject* parent ) :
	QObject( parent )
{
	s_instance = this;

#ifdef Q_OS_ANDROID
	QJniEnvironment env;
	const JNINativeMethod methods[] = {
		{ "nativeQrScanned", "(Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void*>( nativeQrScanned ) }
	};
	env.registerNativeMethods( "id/arunika/arunicontrol/NetworkHelper", methods, 1 );
#endif

	load();
	for( const auto& site : std::as_const( m_sites ) )
	{
		startLink( site );
	}
}



GatewayManager::~GatewayManager()
{
	s_instance = nullptr;
}



QVariantList GatewayManager::sites() const
{
	QVariantList list;
	for( const auto& site : m_sites )
	{
		const auto link = m_links.value( site.gatewayId );
		list.append( QVariantMap{
			{ QStringLiteral("id"), site.gatewayId },
			{ QStringLiteral("name"), link ? link->site().name : site.name },
			{ QStringLiteral("state"), link ? link->state() : QStringLiteral("offline") },
			{ QStringLiteral("error"), link ? link->error() : QString{} },
			{ QStringLiteral("computers"), link ? int( link->hosts().size() ) : 0 },
			{ QStringLiteral("onSite"), link ? link->isOnSite() : false },
			{ QStringLiteral("pending"), site.pairingToken.isEmpty() == false },
		} );
	}
	return list;
}



bool GatewayManager::isScanSupported() const
{
#ifdef Q_OS_ANDROID
	return true;
#else
	return false;
#endif
}



QString GatewayManager::addFromCode( const QString& code )
{
	const auto info = AruniTunnel::PairingInfo::decode( code );
	if( info.isValid() == false )
	{
		return tr("Kode tidak dikenali. Gunakan kode QR dari halaman \"Aruni Gateway\" di AruniControl Configurator.");
	}

	if( info.enrollment )
	{
		return tr("Ini kode pendaftaran laptop, bukan kode untuk HP. Masukkan kode ini di laptop (Configurator → Aruni Gateway), "
				  "lalu pindai kode QR \"Show pairing QR code\" di HP.");
	}

	// the same link may arrive twice (deep link + URL handler) - ignore repeats
	for( const auto& site : std::as_const( m_sites ) )
	{
		if( site.gatewayId == info.gatewayId && site.pairingToken == info.token )
		{
			return {};
		}
	}

	// pairing again with an existing gateway replaces the old entry
	removeSite( info.gatewayId );

	GatewaySite site;
	site.relayUrl = info.relayUrl;
	site.gatewayId = info.gatewayId;
	site.name = info.siteName.isEmpty() ? info.gatewayId : info.siteName;
	site.publicKey = info.gatewayPublicKey;
	site.pairingToken = info.token;
	m_sites.append( site );
	save();

	startLink( site );
	Q_EMIT sitesChanged();
	return {};
}



void GatewayManager::removeSite( const QString& gatewayId )
{
	if( auto link = m_links.take( gatewayId ) )
	{
		link->stop();
		link->deleteLater();
	}

	const auto before = m_sites.size();
	m_sites.removeIf( [&gatewayId]( const GatewaySite& site ) { return site.gatewayId == gatewayId; } );
	if( m_sites.size() != before )
	{
		save();
		Q_EMIT sitesChanged();
		Q_EMIT remoteHostsChanged();
	}
}



void GatewayManager::reconnect( const QString& gatewayId )
{
	if( auto link = m_links.value( gatewayId ) )
	{
		link->stop();
		link->start();
	}
}



void GatewayManager::scanQrCode()
{
#ifdef Q_OS_ANDROID
	QJniObject::callStaticMethod<void>( "id/arunika/arunicontrol/NetworkHelper", "scanQrCode",
										"(Landroid/content/Context;)V",
										QJniObject( QNativeInterface::QAndroidApplication::context() ).object() );
#endif
}



void GatewayManager::handleScannedCode( const QString& code, const QString& error )
{
	if( code.isEmpty() )
	{
		if( error.isEmpty() == false )
		{
			Q_EMIT notify( tr("Pemindai QR tidak tersedia: %1").arg( error ), QStringLiteral("error") );
		}
		return;
	}

	const auto problem = addFromCode( code );
	if( problem.isEmpty() )
	{
		Q_EMIT siteAdded( AruniTunnel::PairingInfo::decode( code ).siteName );
	}
	else
	{
		Q_EMIT notify( problem, QStringLiteral("error") );
	}
}



QList<QPair<GatewaySite, QJsonArray>> GatewayManager::remoteHosts() const
{
	QList<QPair<GatewaySite, QJsonArray>> result;
	for( const auto link : m_links )
	{
		// on the gateway's own network the computers are discovered directly
		if( link->isOnSite() == false && link->hosts().isEmpty() == false )
		{
			result.append( { link->site(), link->hosts() } );
		}
	}
	return result;
}



bool GatewayManager::redirect( const QString& host, int port, QString& redirectedHost, int& redirectedPort )
{
	int localPort = -1;

	const auto resolve = [&]() {
		if( auto link = linkCovering( host ) )
		{
			localPort = link->localPortFor( host, port );
		}
	};

	if( QThread::currentThread() == thread() )
	{
		resolve();
	}
	else
	{
		QMetaObject::invokeMethod( this, resolve, Qt::BlockingQueuedConnection );
	}

	if( localPort <= 0 )
	{
		return false;
	}

	redirectedHost = QStringLiteral("127.0.0.1");
	redirectedPort = localPort;
	return true;
}



bool GatewayManager::wake( const QString& host, const QString& macAddress )
{
	if( auto link = linkCovering( host ) )
	{
		link->wake( macAddress );
		return true;
	}
	return false;
}



void GatewayManager::load()
{
	auto settings = gatewaySettings();

	m_device = AruniTunnel::KeyPair::fromPrivateKey( AruniTunnel::fromBase64Url( settings.value( QStringLiteral("GatewayDeviceKey") ).toString() ) );
	if( m_device.isValid() == false )
	{
		m_device = AruniTunnel::KeyPair::generate();
		settings.setValue( QStringLiteral("GatewayDeviceKey"), AruniTunnel::toBase64Url( m_device.privateKey ) );
	}

	const auto count = settings.beginReadArray( QStringLiteral("GatewaySites") );
	for( int i = 0; i < count; ++i )
	{
		settings.setArrayIndex( i );
		GatewaySite site;
		site.relayUrl = settings.value( QStringLiteral("relay") ).toString();
		site.gatewayId = settings.value( QStringLiteral("id") ).toString();
		site.name = settings.value( QStringLiteral("name") ).toString();
		site.publicKey = AruniTunnel::fromBase64Url( settings.value( QStringLiteral("key") ).toString() );
		site.pairingToken = AruniTunnel::fromBase64Url( settings.value( QStringLiteral("token") ).toString() );
		if( site.gatewayId.isEmpty() == false && site.publicKey.size() == AruniTunnel::KeySize )
		{
			m_sites.append( site );
		}
	}
	settings.endArray();
}



void GatewayManager::save()
{
	auto settings = gatewaySettings();
	settings.beginWriteArray( QStringLiteral("GatewaySites"), int( m_sites.size() ) );
	for( int i = 0; i < m_sites.size(); ++i )
	{
		const auto& site = m_sites.at( i );
		settings.setArrayIndex( i );
		settings.setValue( QStringLiteral("relay"), site.relayUrl );
		settings.setValue( QStringLiteral("id"), site.gatewayId );
		settings.setValue( QStringLiteral("name"), site.name );
		settings.setValue( QStringLiteral("key"), AruniTunnel::toBase64Url( site.publicKey ) );
		settings.setValue( QStringLiteral("token"), AruniTunnel::toBase64Url( site.pairingToken ) );
	}
	settings.endArray();
}



void GatewayManager::startLink( const GatewaySite& site )
{
	auto link = new GatewayLink( this, site, m_device );
	m_links.insert( site.gatewayId, link );

	connect( link, &GatewayLink::stateChanged, this, [this, link]() {
		Q_EMIT sitesChanged();
		if( link->isOnline() )
		{
			Q_EMIT remoteHostsChanged();
		}
	} );
	connect( link, &GatewayLink::hostsChanged, this, [this]() {
		Q_EMIT sitesChanged();
		Q_EMIT remoteHostsChanged();
	} );
	connect( link, &GatewayLink::paired, this, [this, link]() {
		for( auto& site : m_sites )
		{
			if( site.gatewayId == link->site().gatewayId )
			{
				site.pairingToken.clear();
				site.name = link->site().name;
			}
		}
		save();
		Q_EMIT notify( tr("Terhubung ke %1").arg( link->site().name ), QStringLiteral("success") );
	} );
	connect( link, &GatewayLink::accessKeyReceived, this, &GatewayManager::accessKeyReceived );

	link->start();
}



GatewayLink* GatewayManager::linkCovering( const QString& host ) const
{
	for( const auto link : m_links )
	{
		if( link->isOnline() && link->isOnSite() == false && link->covers( host ) )
		{
			return link;
		}
	}
	return nullptr;
}
