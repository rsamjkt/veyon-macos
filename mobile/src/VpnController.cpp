/*
 * VpnController.cpp - remote access (WireGuard tunnel, ZeroTier, extra subnets)
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
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/private/qandroidextras_p.h>
#endif

#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "VpnController.h"


VpnController* VpnController::s_instance = nullptr;

namespace {

constexpr auto JavaHelper = "id/arunika/arunicontrol/NetworkHelper";
constexpr auto ZeroTierPackage = "com.zerotier.one";
constexpr int VpnConsentRequestCode = 4711;

#ifdef Q_OS_ANDROID
QJniObject androidContext()
{
	return QJniObject( QNativeInterface::QAndroidApplication::context() );
}

void JNICALL nativeTunnelStateChanged( JNIEnv*, jclass, jboolean up, jstring error )
{
	const auto message = error ? QJniObject( error ).toString() : QString{};
	if( auto controller = VpnController::instance() )
	{
		QMetaObject::invokeMethod( controller, [controller, up, message]() {
			controller->onTunnelStateChanged( up, message );
		}, Qt::QueuedConnection );
	}
}
#endif

QSettings vpnSettings()
{
	const auto path = QStandardPaths::writableLocation( QStandardPaths::AppDataLocation );
	QDir().mkpath( path );
	return QSettings{ QDir( path ).filePath( QStringLiteral("mobile.ini") ), QSettings::IniFormat };
}

}



VpnController::VpnController( QObject* parent ) :
	QObject( parent )
{
	s_instance = this;

#ifdef Q_OS_ANDROID
	QJniEnvironment env;
	const JNINativeMethod methods[] = {
		{ "nativeTunnelStateChanged", "(ZLjava/lang/String;)V", reinterpret_cast<void*>( nativeTunnelStateChanged ) }
	};
	env.registerNativeMethods( JavaHelper, methods, 1 );
#endif

	QFile file( configPath() );
	if( file.open( QFile::ReadOnly ) )
	{
		m_config = QString::fromUtf8( file.readAll() );
	}

	refresh();

	if( hasConfig() && autoConnect() && m_state == QStringLiteral("off") )
	{
		connectTunnel();
	}
	else
	{
		applyDiscoverySubnets();
	}
}



bool VpnController::isSupported() const
{
#ifdef Q_OS_ANDROID
	return true;
#else
	return false;
#endif
}



QString VpnController::endpoint() const
{
	return value( QStringLiteral("Peer"), QStringLiteral("Endpoint") );
}



QString VpnController::address() const
{
	return value( QStringLiteral("Interface"), QStringLiteral("Address") );
}



QStringList VpnController::tunnelSubnets() const
{
	// the LANs behind the tunnel worth sweeping - neither a default route nor
	// single hosts (/32)
	QStringList subnets;
	const auto allowed = value( QStringLiteral("Peer"), QStringLiteral("AllowedIPs") ).split( QLatin1Char(','), Qt::SkipEmptyParts );
	for( const auto& entry : allowed )
	{
		const auto parsed = QHostAddress::parseSubnet( entry.trimmed() );
		if( parsed.first.protocol() == QAbstractSocket::IPv4Protocol && parsed.second >= 20 && parsed.second <= 30 )
		{
			subnets.append( entry.trimmed() );
		}
	}
	return subnets;
}



QStringList VpnController::extraSubnets() const
{
	return vpnSettings().value( QStringLiteral("ExtraSubnets") ).toStringList();
}



void VpnController::setExtraSubnets( const QStringList& subnets )
{
	QStringList valid;
	for( const auto& subnet : subnets )
	{
		if( validateSubnet( subnet ).isEmpty() && valid.contains( subnet.trimmed() ) == false )
		{
			valid.append( subnet.trimmed() );
		}
	}

	vpnSettings().setValue( QStringLiteral("ExtraSubnets"), valid );
	Q_EMIT extraSubnetsChanged();
	applyDiscoverySubnets();
}



bool VpnController::isOtherVpnActive() const
{
#ifdef Q_OS_ANDROID
	const bool anyVpn = QJniObject::callStaticMethod<jboolean>( JavaHelper, "isAnyVpnActive",
																"(Landroid/content/Context;)Z", androidContext().object() );
	return anyVpn && m_state != QStringLiteral("on");
#else
	return false;
#endif
}



bool VpnController::isZeroTierInstalled() const
{
#ifdef Q_OS_ANDROID
	return QJniObject::callStaticMethod<jboolean>( JavaHelper, "isAppInstalled",
												   "(Landroid/content/Context;Ljava/lang/String;)Z",
												   androidContext().object(),
												   QJniObject::fromString( QString::fromLatin1( ZeroTierPackage ) ).object<jstring>() );
#else
	return false;
#endif
}



bool VpnController::autoConnect() const
{
	return vpnSettings().value( QStringLiteral("VpnAutoConnect"), true ).toBool();
}



void VpnController::setAutoConnect( bool enabled )
{
	vpnSettings().setValue( QStringLiteral("VpnAutoConnect"), enabled );
	Q_EMIT configChanged();
}



QString VpnController::importConfigText( const QString& text )
{
	auto config = text.trimmed();
	config.replace( QStringLiteral("\r\n"), QStringLiteral("\n") );

	if( config.contains( QStringLiteral("[Interface]") ) == false || config.contains( QStringLiteral("[Peer]") ) == false )
	{
		return tr("Ini bukan konfigurasi WireGuard. Bagian [Interface] dan [Peer] harus ada.");
	}

	config = withOwnPackageOnly( config );

#ifdef Q_OS_ANDROID
	const auto error = QJniObject::callStaticObjectMethod( JavaHelper, "validateConfig",
														   "(Ljava/lang/String;)Ljava/lang/String;",
														   QJniObject::fromString( config ).object<jstring>() );
	if( error.isValid() && error.toString().isEmpty() == false )
	{
		return tr("Konfigurasi tidak valid: %1").arg( error.toString() );
	}
#endif

	const bool wasUp = m_state == QStringLiteral("on") || m_state == QStringLiteral("connecting");

	QFile file( configPath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		return tr("Konfigurasi tidak bisa disimpan.");
	}
	file.write( config.toUtf8() );
	file.close();
	file.setPermissions( QFile::ReadOwner | QFile::WriteOwner );

	m_config = config;
	Q_EMIT configChanged();

	if( wasUp )
	{
		disconnectTunnel();
	}
	connectTunnel();

	return {};
}



QString VpnController::importConfigFile( const QUrl& fileUrl )
{
	QFile file( fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString() );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return tr("File tidak bisa dibuka.");
	}

	return importConfigText( QString::fromUtf8( file.read( 64 * 1024 ) ) );
}



void VpnController::removeConfig()
{
	disconnectTunnel();
	QFile::remove( configPath() );
	m_config.clear();
	Q_EMIT configChanged();
	applyDiscoverySubnets();
}



void VpnController::connectTunnel()
{
	if( hasConfig() == false || isSupported() == false )
	{
		return;
	}

	setState( QStringLiteral("connecting") );

#ifdef Q_OS_ANDROID
	const auto intent = QJniObject::callStaticObjectMethod( JavaHelper, "vpnConsentIntent",
															"(Landroid/content/Context;)Landroid/content/Intent;",
															androidContext().object() );
	if( intent.isValid() == false )
	{
		startTunnel();
		return;
	}

	// Android asks the user once whether this app may create a VPN
	QtAndroidPrivate::startActivity( intent, VpnConsentRequestCode, [this]( int, int resultCode, const QJniObject& ) {
		QMetaObject::invokeMethod( this, [this, resultCode]() {
			if( resultCode == -1 ) // Activity.RESULT_OK
			{
				startTunnel();
			}
			else
			{
				setState( QStringLiteral("error"), tr("Izin VPN tidak diberikan.") );
			}
		}, Qt::QueuedConnection );
	} );
#endif
}



void VpnController::disconnectTunnel()
{
#ifdef Q_OS_ANDROID
	QJniObject::callStaticMethod<void>( JavaHelper, "tunnelDown", "(Landroid/content/Context;)V", androidContext().object() );
#endif
	setState( QStringLiteral("off") );
	applyDiscoverySubnets();
}



void VpnController::refresh()
{
#ifdef Q_OS_ANDROID
	const bool up = QJniObject::callStaticMethod<jboolean>( JavaHelper, "isTunnelUp",
															"(Landroid/content/Context;)Z", androidContext().object() );
	if( up && m_state != QStringLiteral("on") )
	{
		setState( QStringLiteral("on") );
	}
#endif
	Q_EMIT stateChanged();
}



void VpnController::openZeroTier()
{
#ifdef Q_OS_ANDROID
	QJniObject::callStaticMethod<void>( JavaHelper, "openApp", "(Landroid/content/Context;Ljava/lang/String;)V",
										androidContext().object(),
										QJniObject::fromString( QString::fromLatin1( ZeroTierPackage ) ).object<jstring>() );
#endif
}



QString VpnController::validateSubnet( const QString& subnet ) const
{
	const auto parsed = QHostAddress::parseSubnet( subnet.trimmed() );
	if( parsed.first.protocol() != QAbstractSocket::IPv4Protocol || parsed.second < 0 )
	{
		return tr("Format harus seperti 192.168.1.0/24");
	}
	if( parsed.second < 20 || parsed.second > 30 )
	{
		return tr("Ukuran jaringan harus antara /20 dan /30");
	}
	return {};
}



void VpnController::onTunnelStateChanged( bool up, const QString& error )
{
	if( up )
	{
		setState( QStringLiteral("on") );
	}
	else if( error.isEmpty() == false )
	{
		QString message = error;
		if( error.contains( QStringLiteral("DNS_RESOLUTION"), Qt::CaseInsensitive ) )
		{
			message = tr("Alamat server VPN (%1) tidak ditemukan. Periksa koneksi internet.").arg( endpoint() );
		}
		else if( error.contains( QStringLiteral("VPN_NOT_AUTHORIZED"), Qt::CaseInsensitive ) )
		{
			message = tr("Izin VPN tidak diberikan.");
		}
		setState( QStringLiteral("error"), message );
	}
	else if( m_state != QStringLiteral("connecting") )
	{
		setState( QStringLiteral("off") );
	}

	applyDiscoverySubnets();
}



QString VpnController::configPath() const
{
	const auto path = QStandardPaths::writableLocation( QStandardPaths::AppDataLocation );
	QDir().mkpath( path );
	return QDir( path ).filePath( QStringLiteral("wireguard.conf") );
}



QString VpnController::value( const QString& section, const QString& key ) const
{
	QString currentSection;
	const auto lines = m_config.split( QLatin1Char('\n') );
	for( const auto& rawLine : lines )
	{
		const auto line = rawLine.trimmed();
		if( line.startsWith( QLatin1Char('[') ) )
		{
			currentSection = line.mid( 1, line.indexOf( QLatin1Char(']') ) - 1 ).trimmed();
		}
		else if( currentSection.compare( section, Qt::CaseInsensitive ) == 0 &&
				 line.section( QLatin1Char('='), 0, 0 ).trimmed().compare( key, Qt::CaseInsensitive ) == 0 )
		{
			return line.section( QLatin1Char('='), 1 ).trimmed();
		}
	}
	return {};
}



QString VpnController::withOwnPackageOnly( const QString& config )
{
	// route only AruniControl through the tunnel so the rest of the phone keeps
	// using the normal internet connection
	static const QRegularExpression appRules( QStringLiteral("^\\s*(IncludedApplications|ExcludedApplications)\\s*=.*$"),
											  QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption );

	auto result = config;
	result.remove( appRules );

	QString packageName = QStringLiteral("id.arunika.arunicontrol");
#ifdef Q_OS_ANDROID
	packageName = androidContext().callObjectMethod<jstring>( "getPackageName" ).toString();
#endif

	const auto interfaceHeader = result.indexOf( QStringLiteral("[Interface]"), 0, Qt::CaseInsensitive );
	if( interfaceHeader >= 0 )
	{
		result.insert( interfaceHeader + int( qstrlen( "[Interface]" ) ),
					   QStringLiteral("\nIncludedApplications = %1").arg( packageName ) );
	}

	result.replace( QRegularExpression( QStringLiteral("\n{3,}") ), QStringLiteral("\n\n") );
	return result;
}



void VpnController::setState( const QString& state, const QString& error )
{
	if( state != m_state || error != m_error )
	{
		m_state = state;
		m_error = error;
		Q_EMIT stateChanged();
	}
}



void VpnController::applyDiscoverySubnets()
{
	auto subnets = extraSubnets();
	if( m_state == QStringLiteral("on") )
	{
		for( const auto& subnet : tunnelSubnets() )
		{
			if( subnets.contains( subnet ) == false )
			{
				subnets.append( subnet );
			}
		}
	}

	auto& config = VeyonCore::config();
	const auto current = config.value( QStringLiteral("ExtraSubnets"), QStringLiteral("NetworkDiscovery"), {} ).toStringList();
	if( current != subnets )
	{
		config.setValue( QStringLiteral("ExtraSubnets"), subnets, QStringLiteral("NetworkDiscovery") );
		config.flushStore();
		Q_EMIT networkRoutesChanged();
	}
}



void VpnController::startTunnel()
{
#ifdef Q_OS_ANDROID
	QJniObject::callStaticMethod<void>( JavaHelper, "tunnelUp", "(Landroid/content/Context;Ljava/lang/String;)V",
										androidContext().object(), QJniObject::fromString( m_config ).object<jstring>() );
#endif
}
