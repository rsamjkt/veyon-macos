/*
 * MobileApp.cpp - backend of the AruniControl Mobile UI
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
#include <QDesktopServices>
#include <QUuid>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QMimeDatabase>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QCoreApplication>
#endif

#include "AuthenticationCredentials.h"
#include "CheckableItemProxyModel.h"
#include "ComputerControlListModel.h"
#include "ComputerGridModel.h"
#include "ComputerManager.h"
#include "ComputerMonitoringModel.h"
#include "CryptoCore.h"
#include "FeatureManager.h"
#include "Filesystem.h"
#include "MobileApp.h"
#include "NetworkObject.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "PlatformFilesystemFunctions.h"
#include "PlatformSessionFunctions.h"
#include "PlatformUserFunctions.h"
#include "UserConfig.h"
#include "VeyonConfiguration.h"
#include "VeyonMaster.h"
#include "VpnController.h"
#include "VncConnection.h"


namespace {

#ifdef Q_OS_ANDROID
jboolean JNICALL nativeHandleLink( JNIEnv*, jclass, jstring link )
{
	const auto url = QUrl( QJniObject( link ).toString() );
	const auto app = QCoreApplication::instance();
	if( app == nullptr )
	{
		return false;
	}
	QMetaObject::invokeMethod( app, [url]() { QDesktopServices::openUrl( url ); }, Qt::QueuedConnection );
	return true;
}
#endif

// uids of the directory plugins shipped in the APK
const auto NetworkDiscoveryPluginUid = QUuid( QStringLiteral("3c5e9a14-2b7d-4e6f-8a1c-9d0f2e4b6c81") );

const auto MobileSettingsGroup = QStringLiteral("Mobile");

QString mobileDataPath()
{
	const auto path = QStandardPaths::writableLocation( QStandardPaths::AppDataLocation );
	QDir().mkpath( path );
	return path;
}

QSettings mobileSettings()
{
	return QSettings{ QDir( mobileDataPath() ).filePath( QStringLiteral("mobile.ini") ), QSettings::IniFormat };
}

// the private key directory is wiped when a new key is imported, so make sure
// it is ours (matters for desktop development builds sharing a machine with a
// real AruniControl installation)
QString privateKeyBaseDir()
{
	const auto baseDir = VeyonCore::filesystem().expandPath( VeyonCore::config().privateKeyBaseDir() );
	return QDir( baseDir ).absolutePath().startsWith( QDir( mobileDataPath() ).absolutePath() ) ? baseDir : QString{};
}

}



MobileApp::MobileApp( VeyonMaster* master, QObject* parent ) :
	QObject( parent ),
	m_master( master ),
	m_computers( new ComputerGridModel( this ) ),
	m_vpn( new VpnController( this ) ),
	m_gateways( new GatewayManager( this ) )
{
	// connections to computers behind a gateway go through its tunnel
	VncConnection::setConnectionRedirector( [gateways = m_gateways]( const QString& host, int port,
																	 QString& redirectedHost, int& redirectedPort ) {
		return GatewayManager::instance() == gateways &&
			   gateways->redirect( host, port, redirectedHost, redirectedPort );
	} );
	connect( m_gateways, &GatewayManager::remoteHostsChanged, this, &MobileApp::publishGatewayHosts );
	connect( m_gateways, &GatewayManager::notify, this, &MobileApp::notify );
	connect( m_gateways, &GatewayManager::siteAdded, this, &MobileApp::gatewayAdded );
	connect( m_gateways, &GatewayManager::accessKeyReceived, this, [this]( const QString& name, const QString& pem ) {
		const auto error = importKeyText( pem, name );
		Q_EMIT notify( error.isEmpty() ? tr("Kunci akses \"%1\" dipasang otomatis dari gateway").arg( name ) : error,
					   error.isEmpty() ? QStringLiteral("success") : QStringLiteral("error") );
	} );
	QDesktopServices::setUrlHandler( QStringLiteral("arunicontrol"), this, "handleUrl" );
#ifdef Q_OS_ANDROID
	{
		QJniEnvironment env;
		const JNINativeMethod methods[] = {
			{ "nativeHandleLink", "(Ljava/lang/String;)Z", reinterpret_cast<void*>( nativeHandleLink ) }
		};
		env.registerNativeMethods( "id/arunika/arunicontrol/AruniActivity", methods, 1 );
		// a link that started the app
		QJniObject::callStaticMethod<void>( "id/arunika/arunicontrol/AruniActivity", "deliverPendingLink" );
	}
#endif

	// computers behind a (new) tunnel or subnet have to be swept for
	connect( m_vpn, &VpnController::networkRoutesChanged, this, &MobileApp::refreshComputers );

	m_computers->setSourceModel( m_master->computerMonitoringModel() );
	m_computers->setLockFeatureUid( featureByName( QStringLiteral("ScreenLock") ).uid() );

	// show every room: new locations appear checked, and computers added to a
	// checked location are checked automatically by CheckableItemProxyModel
	auto treeModel = m_master->computerManager().computerTreeModel();
	connect( treeModel, &QAbstractItemModel::rowsInserted, this, [this]( const QModelIndex& parent ) {
		if( parent.isValid() == false )
		{
			checkAllLocations();
		}
	} );
	connect( treeModel, &QAbstractItemModel::modelReset, this, &MobileApp::checkAllLocations );
	checkAllLocations();

	// files collected from the computers end up in the app's own storage
	VeyonCore::config().setValue( QStringLiteral("CollectedFilesDestinationDirectory"), collectedFilesDirectory(),
								  QStringLiteral("FileTransfer") );

	// restore remembered logon credentials
	auto settings = mobileSettings();
	if( VeyonCore::config().authenticationMethod() == VeyonCore::AuthenticationMethod::LogonAuthentication )
	{
		const auto username = settings.value( QStringLiteral("LogonUser") ).toString();
		const auto password = settings.value( QStringLiteral("LogonPassword") ).toString();
		if( username.isEmpty() == false && password.isEmpty() == false )
		{
			VeyonCore::authenticationCredentials().setLogonUsername( username );
			VeyonCore::authenticationCredentials().setLogonPassword( VeyonCore::cryptoCore().decryptPassword( password ) );
		}
	}
	else
	{
		VeyonCore::instance()->initAuthentication();
	}
}



MobileApp::~MobileApp() = default;



void MobileApp::applyDefaults()
{
#ifdef Q_OS_ANDROID
	// log to logcat ("adb logcat -s AruniControl") - takes effect from the next start
	const auto logDirectory = VeyonCore::platform().filesystemFunctions().globalTempPath();
	if( VeyonCore::config().logToSystem() == false || VeyonCore::config().logLevel() != Logger::LogLevel::Info ||
		VeyonCore::config().logFileDirectory() != logDirectory )
	{
		// /data/user/0 is a symlink, which Veyon's logger refuses
		VeyonCore::config().setLogFileDirectory( logDirectory );
		VeyonCore::config().setLogToSystem( true );
		VeyonCore::config().setLogLevel( Logger::LogLevel::Info );
		VeyonCore::config().flushStore();
	}
#endif

	auto settings = mobileSettings();
	if( settings.value( QStringLiteral("Initialized") ).toBool() )
	{
		return;
	}

	auto& config = VeyonCore::config();
	config.setPrivateKeyBaseDir( QDir( mobileDataPath() ).filePath( QStringLiteral("keys/private") ) );
	config.setPublicKeyBaseDir( QDir( mobileDataPath() ).filePath( QStringLiteral("keys/public") ) );
	config.setUserConfigurationDirectory( QDir( mobileDataPath() ).filePath( QStringLiteral("config") ) );
	config.setNetworkObjectDirectoryPlugin( NetworkDiscoveryPluginUid );
	config.setAuthenticationMethod( VeyonCore::AuthenticationMethod::KeyFileAuthentication );
	config.setComputerDisplayRoleContent( ComputerListModel::DisplayRoleContent::ComputerName );
	config.setAutoSelectCurrentLocation( false );
	// a phone screen does not need lossless images - JPEG saves a lot of data
	config.setRemoteAccessImageQuality( VncConnectionConfiguration::Quality::High );
	config.setComputerMonitoringImageQuality( VncConnectionConfiguration::Quality::Medium );
	config.setShowCurrentLocationOnly( false );
	config.flushStore();

	UserConfig userConfig( QStringLiteral("VeyonMaster") );
	// thumbnails sharp enough for a two-column grid on a high-DPI phone
	userConfig.setMonitoringScreenSize( 640 );
	userConfig.flushStore();

	settings.setValue( QStringLiteral("Initialized"), true );

	// the built-in access control provider already instantiated the directory
	// configured before these defaults were applied
	VeyonCore::networkObjectDirectoryManager().reloadConfiguredDirectory();
}



bool MobileApp::isAuthenticated() const
{
	const auto& credentials = VeyonCore::authenticationCredentials();
	switch( VeyonCore::config().authenticationMethod() )
	{
	case VeyonCore::AuthenticationMethod::KeyFileAuthentication:
		return credentials.hasCredentials( AuthenticationCredentials::Type::PrivateKey );
	case VeyonCore::AuthenticationMethod::LogonAuthentication:
		return credentials.hasCredentials( AuthenticationCredentials::Type::UserLogon );
	}

	return false;
}



QString MobileApp::authMethod() const
{
	if( isAuthenticated() == false )
	{
		return {};
	}

	return VeyonCore::config().authenticationMethod() == VeyonCore::AuthenticationMethod::KeyFileAuthentication ?
			   QStringLiteral("key") : QStringLiteral("logon");
}



QString MobileApp::authName() const
{
	if( isAuthenticated() == false )
	{
		return {};
	}

	const auto& credentials = VeyonCore::authenticationCredentials();
	return VeyonCore::config().authenticationMethod() == VeyonCore::AuthenticationMethod::KeyFileAuthentication ?
			   credentials.authenticationKeyName() : credentials.logonUsername();
}



QVariantList MobileApp::rooms() const
{
	const auto objects = directoryObjects();
	const auto managed = managedLocationUids();

	QVariantList rooms;
	for( const auto& value : objects )
	{
		const NetworkObject room( value.toObject() );
		if( room.type() != NetworkObject::Type::Location || managed.contains( room.uid().toString() ) )
		{
			continue;
		}

		QVariantList computers;
		for( const auto& childValue : objects )
		{
			const NetworkObject computer( childValue.toObject() );
			if( computer.type() == NetworkObject::Type::Host && computer.parentUid() == room.uid() )
			{
				computers.append( QVariantMap{
					{ QStringLiteral("uid"), computer.uid().toString() },
					{ QStringLiteral("name"), computer.name() },
					{ QStringLiteral("host"), computer.hostAddress() },
					{ QStringLiteral("mac"), computer.macAddress() },
				} );
			}
		}

		rooms.append( QVariantMap{
			{ QStringLiteral("uid"), room.uid().toString() },
			{ QStringLiteral("name"), room.name() },
			{ QStringLiteral("computers"), computers },
		} );
	}

	return rooms;
}



QString MobileApp::networkAddress() const
{
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& iface : interfaces )
	{
		const auto flags = iface.flags();
		if( flags.testFlag( QNetworkInterface::IsUp ) == false ||
			flags.testFlag( QNetworkInterface::IsLoopBack ) )
		{
			continue;
		}

		for( const auto& entry : iface.addressEntries() )
		{
			if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol && entry.ip().isLoopback() == false )
			{
				return QStringLiteral("%1/%2").arg( entry.ip().toString() ).arg( entry.prefixLength() );
			}
		}
	}

	return {};
}



QString MobileApp::version() const
{
	return VeyonCore::versionString();
}



QString MobileApp::deviceName() const
{
	return VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::FullName );
}



QString MobileApp::screenshotDirectory() const
{
	return QDir( mobileDataPath() ).filePath( QStringLiteral("screenshots") );
}



QString MobileApp::suggestKeyName( const QUrl& fileUrl ) const
{
	// the Configurator exports "<name>_private_key.pem"; key stores use "<name>/key"
	// for content:// URIs Qt's Android file engine resolves the document's
	// display name (the URI itself often only carries a numeric document id)
	auto fileName = fileUrl.isLocalFile() ? fileUrl.fileName() : QFileInfo( fileUrl.toString() ).fileName();
	if( fileName.isEmpty() || fileName.contains( QLatin1Char(':') ) || fileName.contains( QLatin1Char('%') ) )
	{
		fileName = QUrl::fromPercentEncoding( fileUrl.toString().section( QLatin1Char('/'), -1 ).toUtf8() )
					   .section( QLatin1Char('/'), -1 ).section( QLatin1Char(':'), -1 );
	}

	auto name = QFileInfo( fileName ).completeBaseName();
	if( name.compare( QStringLiteral("key"), Qt::CaseInsensitive ) == 0 && fileUrl.isLocalFile() )
	{
		name = QFileInfo( fileUrl.toLocalFile() ).dir().dirName();
	}

	name.remove( QRegularExpression( QStringLiteral("[_-]?(private|public)[_-]?key.*$"), QRegularExpression::CaseInsensitiveOption ) );
	name.remove( QRegularExpression( QStringLiteral("\\s*\\(\\d+\\)$") ) ); // "name (1)" from repeated downloads
	name.replace( QRegularExpression( QStringLiteral("[^A-Za-z0-9_-]") ), QStringLiteral("_") );

	return name;
}



QString MobileApp::collectedFilesDirectory() const
{
	return QDir( mobileDataPath() ).filePath( QStringLiteral("collected") );
}



QString MobileApp::displayName( const QUrl& fileUrl ) const
{
	return fileUrl.isLocalFile() ? fileUrl.fileName() : QFileInfo( fileUrl.toString() ).fileName();
}



QString MobileApp::importKeyFile( const QUrl& fileUrl, const QString& keyName )
{
	// content:// URIs (Android document picker) are opened by QFile directly
	const auto path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();

	QFile file( path );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return tr("File kunci tidak bisa dibuka.");
	}

	const auto pem = QString::fromUtf8( file.read( 64 * 1024 ) );

	return importKeyText( pem, keyName.trimmed().isEmpty() ? suggestKeyName( fileUrl ) : keyName.trimmed() );
}



QString MobileApp::importKeyText( const QString& pem, const QString& keyName )
{
	if( pem.contains( QStringLiteral("PUBLIC KEY") ) && pem.contains( QStringLiteral("PRIVATE KEY") ) == false )
	{
		return tr("Ini kunci publik. Yang dibutuhkan adalah kunci privat (…_private_key.pem).");
	}

	QCA::ConvertResult result = QCA::ErrorDecode;
	const auto key = CryptoCore::PrivateKey::fromPEM( pem, {}, &result );
	if( result != QCA::ConvertGood || key.isNull() || key.isPrivate() == false )
	{
		return tr("File ini bukan kunci privat AruniControl yang valid.");
	}

	// make sure this device can actually sign with the key (this is what the
	// computers verify during authentication)
	{
		const auto challenge = CryptoCore::generateChallenge();
		auto signingKey = key;
		const auto signature = signingKey.signMessage( challenge, CryptoCore::DefaultSignatureAlgorithm );
		auto publicKey = key.toPublicKey();
		const auto verified = signature.isEmpty() == false &&
							  publicKey.verifyMessage( challenge, signature, CryptoCore::DefaultSignatureAlgorithm );
		vInfo() << "key self-test: signature size" << signature.size() << "verified" << verified;
		if( verified == false )
		{
			return tr("Kunci ini tidak bisa dipakai untuk tanda tangan di perangkat ini.");
		}
	}

	const auto name = keyName;
	if( VeyonCore::isAuthenticationKeyNameValid( name ) == false )
	{
		return tr("Nama kunci hanya boleh berisi huruf, angka, - dan _.");
	}

	// keep a single private key so auto-detection picks the right one
	const auto baseDir = privateKeyBaseDir();
	if( baseDir.isEmpty() )
	{
		return tr("Folder kunci tidak valid.");
	}
	QDir( baseDir ).removeRecursively();

	const auto keyPath = VeyonCore::filesystem().privateKeyPath( name );
	QDir().mkpath( QFileInfo( keyPath ).absolutePath() );
	if( key.toPEMFile( keyPath ) == false )
	{
		return tr("Kunci tidak bisa disimpan di perangkat ini.");
	}
	QFile::setPermissions( keyPath, QFile::ReadOwner | QFile::WriteOwner );

	VeyonCore::config().setAuthenticationMethod( VeyonCore::AuthenticationMethod::KeyFileAuthentication );
	saveConfiguration();

	if( VeyonCore::instance()->initAuthentication() == false )
	{
		return tr("Kunci tersimpan, tapi gagal dimuat.");
	}

	auto settings = mobileSettings();
	settings.remove( QStringLiteral("LogonUser") );
	settings.remove( QStringLiteral("LogonPassword") );

	reconnectAll();
	Q_EMIT authenticationChanged();
	return {};
}



QString MobileApp::useLogon( const QString& username, const QString& password, bool remember )
{
	if( username.trimmed().isEmpty() || password.isEmpty() )
	{
		return tr("Isi nama pengguna dan kata sandi.");
	}

	VeyonCore::config().setAuthenticationMethod( VeyonCore::AuthenticationMethod::LogonAuthentication );
	saveConfiguration();

	const auto securePassword = CryptoCore::PlaintextPassword( password.toUtf8() );
	VeyonCore::authenticationCredentials().setLogonUsername( username.trimmed() );
	VeyonCore::authenticationCredentials().setLogonPassword( securePassword );

	auto settings = mobileSettings();
	if( remember )
	{
		settings.setValue( QStringLiteral("LogonUser"), username.trimmed() );
		settings.setValue( QStringLiteral("LogonPassword"), VeyonCore::cryptoCore().encryptPassword( securePassword ) );
	}
	else
	{
		settings.remove( QStringLiteral("LogonUser") );
		settings.remove( QStringLiteral("LogonPassword") );
	}

	reconnectAll();
	Q_EMIT authenticationChanged();
	return {};
}



void MobileApp::signOut()
{
	const auto baseDir = privateKeyBaseDir();
	if( baseDir.isEmpty() == false )
	{
		QDir( baseDir ).removeRecursively();
	}

	auto settings = mobileSettings();
	settings.remove( QStringLiteral("LogonUser") );
	settings.remove( QStringLiteral("LogonPassword") );

	VeyonCore::authenticationCredentials().setLogonUsername( {} );
	VeyonCore::authenticationCredentials().setLogonPassword( {} );
	VeyonCore::authenticationCredentials().setPrivateKey( {} );

	reconnectAll();
	Q_EMIT authenticationChanged();
}



QString MobileApp::addRoom( const QString& name )
{
	if( name.trimmed().isEmpty() )
	{
		return {};
	}

	const NetworkObject room( NetworkObject::Type::Location, name.trimmed(), {}, {}, {}, QUuid::createUuid() );

	auto objects = directoryObjects();
	objects.append( room.toJson() );
	setDirectoryObjects( objects );

	return room.uid().toString();
}



void MobileApp::renameRoom( const QString& roomUid, const QString& name )
{
	auto objects = directoryObjects();
	for( auto&& value : objects )
	{
		auto object = value.toObject();
		if( object[QStringLiteral("Uid")].toString() == roomUid )
		{
			object[QStringLiteral("Name")] = name.trimmed();
			value = object;
		}
	}
	setDirectoryObjects( objects );
}



void MobileApp::removeRoom( const QString& roomUid )
{
	QJsonArray remaining;
	for( const auto& value : directoryObjects() )
	{
		const auto object = value.toObject();
		if( object[QStringLiteral("Uid")].toString() != roomUid &&
			object[QStringLiteral("ParentUid")].toString() != roomUid )
		{
			remaining.append( object );
		}
	}
	setDirectoryObjects( remaining );
}



QString MobileApp::addComputer( const QString& roomUid, const QString& name, const QString& host, const QString& mac )
{
	const auto hostAddress = host.trimmed();
	if( hostAddress.isEmpty() )
	{
		return tr("Isi alamat IP atau nama host komputer.");
	}

	const NetworkObject computer( NetworkObject::Type::Host,
								  name.trimmed().isEmpty() ? hostAddress : name.trimmed(),
								  hostAddress, mac.trimmed().toUpper(), {},
								  QUuid::createUuid(), QUuid( roomUid ) );

	auto objects = directoryObjects();
	objects.append( computer.toJson() );
	setDirectoryObjects( objects );

	return {};
}



void MobileApp::removeComputer( const QString& computerUid )
{
	QJsonArray remaining;
	for( const auto& value : directoryObjects() )
	{
		if( value.toObject()[QStringLiteral("Uid")].toString() != computerUid )
		{
			remaining.append( value );
		}
	}
	setDirectoryObjects( remaining );
}



void MobileApp::refreshComputers()
{
	if( auto directory = m_master->computerManager().networkObjectDirectory() )
	{
		directory->update();
	}

	Q_EMIT networkChanged();
}



bool MobileApp::hasFeature( const QString& name ) const
{
	return featureByName( name ).isValid();
}



bool MobileApp::runFeature( const QString& name, bool start, const QVariantMap& arguments, const QStringList& uids )
{
	const auto feature = featureByName( name );
	const auto interfaces = targets( uids );

	if( feature.isValid() == false || interfaces.isEmpty() )
	{
		return false;
	}

	VeyonCore::featureManager().controlFeature( feature.uid(),
												start ? FeatureProviderInterface::Operation::Start :
														FeatureProviderInterface::Operation::Stop,
												arguments, interfaces );
	return true;
}



void MobileApp::lockScreens( bool lock, const QStringList& uids )
{
	if( runFeature( QStringLiteral("ScreenLock"), lock, {}, uids ) )
	{
		Q_EMIT notify( lock ? tr("Layar dikunci di %1 komputer").arg( targetCount( uids ) ) :
							  tr("Kunci layar dibuka di %1 komputer").arg( targetCount( uids ) ),
					   QStringLiteral("success") );
	}
}



void MobileApp::sendMessage( const QString& title, const QString& text, const QStringList& uids )
{
	// 1 == QMessageBox::Information
	if( runFeature( QStringLiteral("TextMessage"), true,
					{ { QStringLiteral("text"), text.toHtmlEscaped().replace( QLatin1Char('\n'), QStringLiteral("<br>") ) },
					  { QStringLiteral("icon"), 1 },
					  { QStringLiteral("title"), title } }, uids ) )
	{
		Q_EMIT notify( tr("Pesan terkirim ke %1 komputer").arg( targetCount( uids ) ), QStringLiteral("success") );
	}
}



void MobileApp::powerAction( const QString& action, const QStringList& uids, int delaySeconds )
{
	static const QHash<QString, QString> features{
		{ QStringLiteral("on"), QStringLiteral("PowerOn") },
		{ QStringLiteral("reboot"), QStringLiteral("Reboot") },
		{ QStringLiteral("off"), QStringLiteral("PowerDownNow") },
		{ QStringLiteral("offDelayed"), QStringLiteral("PowerDownDelayed") },
		{ QStringLiteral("logoff"), QStringLiteral("UserLogoff") },
	};

	if( action == QStringLiteral("on") )
	{
		// computers behind a gateway: the magic packet has to be sent on their LAN
		for( const auto& controlInterface : targets( uids ) )
		{
			const auto mac = controlInterface->computer().macAddress();
			if( mac.isEmpty() == false )
			{
				m_gateways->wake( controlInterface->computer().hostAddress().toString().isEmpty() ?
									  controlInterface->computer().hostName() :
									  controlInterface->computer().hostAddress().toString(), mac );
			}
		}
	}

	QVariantMap arguments;
	if( action == QStringLiteral("offDelayed") )
	{
		arguments[QStringLiteral("shutdownTimeout")] = delaySeconds;
	}

	if( runFeature( features.value( action ), true, arguments, uids ) )
	{
		static const QHash<QString, QString> messages{
			{ QStringLiteral("on"), tr("Perintah menyalakan dikirim ke %1 komputer") },
			{ QStringLiteral("reboot"), tr("%1 komputer sedang dimulai ulang") },
			{ QStringLiteral("off"), tr("%1 komputer sedang dimatikan") },
			{ QStringLiteral("offDelayed"), tr("%1 komputer akan mati sesuai hitung mundur") },
			{ QStringLiteral("logoff"), tr("Pengguna di %1 komputer dikeluarkan") },
		};
		Q_EMIT notify( messages.value( action ).arg( targetCount( uids ) ), QStringLiteral("success") );
	}
}



void MobileApp::openWebsite( const QString& url, const QStringList& uids )
{
	auto website = url.trimmed();
	if( website.isEmpty() )
	{
		return;
	}
	if( website.contains( QStringLiteral("://") ) == false )
	{
		website.prepend( QStringLiteral("https://") );
	}

	if( runFeature( QStringLiteral("OpenWebsite"), true, { { QStringLiteral("websiteUrls"), QStringList{ website } } }, uids ) )
	{
		Q_EMIT notify( tr("Website dibuka di %1 komputer").arg( targetCount( uids ) ), QStringLiteral("success") );
	}
}



void MobileApp::startApplication( const QString& command, const QStringList& uids )
{
	if( command.trimmed().isEmpty() )
	{
		return;
	}

	if( runFeature( QStringLiteral("StartApp"), true, { { QStringLiteral("applications"), QStringList{ command.trimmed() } } }, uids ) )
	{
		Q_EMIT notify( tr("Aplikasi dijalankan di %1 komputer").arg( targetCount( uids ) ), QStringLiteral("success") );
	}
}



void MobileApp::setInternetBlocked( bool blocked, const QStringList& uids )
{
	if( runFeature( QStringLiteral("InternetAccessControl"), blocked, {}, uids ) )
	{
		Q_EMIT notify( blocked ? tr("Internet diblokir di %1 komputer").arg( targetCount( uids ) ) :
								 tr("Internet dibuka kembali di %1 komputer").arg( targetCount( uids ) ),
					   QStringLiteral("success") );
	}
}



void MobileApp::setAudioMuted( bool muted, const QStringList& uids )
{
	if( runFeature( QStringLiteral("AruniMediaMute"), muted, {}, uids ) )
	{
		Q_EMIT notify( muted ? tr("Suara dibisukan di %1 komputer").arg( targetCount( uids ) ) :
							   tr("Suara diaktifkan di %1 komputer").arg( targetCount( uids ) ),
					   QStringLiteral("success") );
	}
}



int MobileApp::saveScreenshots( const QStringList& uids )
{
	const auto directory = screenshotDirectory();
	QDir().mkpath( directory );

	int saved = 0;
	const auto timestamp = QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") );
	for( const auto& controlInterface : targets( uids ) )
	{
		const auto image = controlInterface->framebuffer();
		if( image.isNull() )
		{
			continue;
		}

		auto name = controlInterface->computerName();
		name.replace( QRegularExpression( QStringLiteral("[^A-Za-z0-9_.-]+") ), QStringLiteral("_") );
		if( image.save( QDir( directory ).filePath( QStringLiteral("%1_%2.png").arg( name, timestamp ) ) ) )
		{
			++saved;
		}
	}

	if( saved > 0 )
	{
		Q_EMIT notify( tr("%1 tangkapan layar disimpan").arg( saved ), QStringLiteral("success") );
	}
	else
	{
		Q_EMIT notify( tr("Tidak ada layar yang bisa ditangkap"), QStringLiteral("error") );
	}

	return saved;
}



void MobileApp::lockInput( bool lock, const QStringList& uids )
{
	if( runFeature( QStringLiteral("InputDevicesLock"), lock, {}, uids ) )
	{
		Q_EMIT notify( lock ? tr("Keyboard & mouse dikunci di %1 komputer").arg( targetCount( uids ) ) :
							  tr("Keyboard & mouse dibuka di %1 komputer").arg( targetCount( uids ) ),
					   QStringLiteral("success") );
	}
}



void MobileApp::loginUser( const QString& username, const QString& password, const QStringList& uids )
{
	if( runFeature( QStringLiteral("UserLogin"), true,
					{ { QStringLiteral("username"), username.trimmed() },
					  { QStringLiteral("password"), password.toUtf8() } }, uids ) )
	{
		Q_EMIT notify( tr("Login %1 dikirim ke %2 komputer").arg( username.trimmed() ).arg( targetCount( uids ) ),
					   QStringLiteral("success") );
	}
}



QString MobileApp::shareScreen( const QString& sourceUid, bool fullScreen, const QStringList& uids )
{
	const auto feature = featureByName( fullScreen ? QStringLiteral("ShareUserScreenFullScreen")
												   : QStringLiteral("ShareUserScreenWindow") );
	const auto source = m_computers->controlInterface( sourceUid );
	if( feature.isValid() == false || source.isNull() )
	{
		return tr("Fitur demo tidak tersedia.");
	}

	if( source->state() != ComputerControlInterface::State::Connected )
	{
		return tr("%1 sedang tidak terhubung.").arg( source->computerName() );
	}

	// the demo plugin shares the screen of the (single) selected computer
	m_master->setSelectedComputerControlInterfaces( { source } );
	VeyonCore::featureManager().startFeature( *m_master, feature, targets( uids ) );

	Q_EMIT notify( tr("Layar %1 ditampilkan ke komputer lain").arg( source->computerName() ), QStringLiteral("success") );
	return {};
}



void MobileApp::stopDemo( const QStringList& uids )
{
	const auto interfaces = targets( uids );
	for( const auto& name : { QStringLiteral("Demo"), QStringLiteral("ShareUserScreenFullScreen"),
							  QStringLiteral("ShareUserScreenWindow") } )
	{
		const auto feature = featureByName( name );
		if( feature.isValid() )
		{
			VeyonCore::featureManager().stopFeature( *m_master, feature, interfaces );
		}
	}

	Q_EMIT notify( tr("Demo dihentikan"), QStringLiteral("success") );
}



QString MobileApp::sendFiles( const QList<QUrl>& files, const QStringList& uids )
{
	// picked documents may be content:// URIs - copy them into the cache under
	// their real names, which the computers will use for the received files
	const auto stagingDir = QDir( QStandardPaths::writableLocation( QStandardPaths::CacheLocation ) ).filePath( QStringLiteral("outgoing") );
	QDir( stagingDir ).removeRecursively();
	QDir().mkpath( stagingDir );

	QStringList localFiles;
	for( const auto& url : files )
	{
		const auto source = url.isLocalFile() ? url.toLocalFile() : url.toString();
		auto name = QFileInfo( source ).fileName();
		if( name.isEmpty() || name.contains( QLatin1Char('%') ) || name.contains( QLatin1Char(':') ) )
		{
			name = QFileInfo( QUrl::fromPercentEncoding( source.toUtf8() ) ).fileName().section( QLatin1Char(':'), -1 );
		}
		if( name.isEmpty() )
		{
			name = QStringLiteral("file-%1").arg( localFiles.size() + 1 );
		}

		const auto target = QDir( stagingDir ).filePath( name );
		QFile::remove( target );
		if( QFile::copy( source, target ) == false )
		{
			return tr("File %1 tidak bisa dibaca.").arg( name );
		}
		localFiles.append( target );
	}

	if( localFiles.isEmpty() )
	{
		return tr("Tidak ada file yang dipilih.");
	}

	if( hasFeature( QStringLiteral("DistributeFiles") ) == false || targets( uids ).isEmpty() )
	{
		return tr("Fitur kirim file tidak tersedia.");
	}

	const auto feature = featureByName( QStringLiteral("DistributeFiles") );
	const auto interfaces = targets( uids );
	VeyonCore::featureManager().controlFeature( feature.uid(), FeatureProviderInterface::Operation::Initialize,
												{ { QStringLiteral("files"), localFiles } }, interfaces );
	VeyonCore::featureManager().controlFeature( feature.uid(), FeatureProviderInterface::Operation::Start,
												{}, interfaces );

	Q_EMIT notify( tr("Mengirim %1 file ke %2 komputer…").arg( localFiles.size() ).arg( interfaces.size() ),
				   QStringLiteral("success") );
	return {};
}



void MobileApp::collectFiles( const QStringList& uids )
{
	QDir().mkpath( collectedFilesDirectory() );
	if( runFeature( QStringLiteral("FileCollect"), true, {}, uids ) )
	{
		Q_EMIT notify( tr("Mengumpulkan file dari %1 komputer…").arg( targetCount( uids ) ), QStringLiteral("success") );
	}
}



QStringList MobileApp::collectedFiles() const
{
	QStringList files;
	QDirIterator it( collectedFilesDirectory(), QDir::Files, QDirIterator::Subdirectories );
	while( it.hasNext() )
	{
		files.append( it.next() );
	}
	files.sort();
	return files;
}



int MobileApp::targetCount( const QStringList& uids ) const
{
	return int( targets( uids ).size() );
}



void MobileApp::shareFile( const QString& path )
{
#ifdef Q_OS_ANDROID
	const auto context = QJniObject( QNativeInterface::QAndroidApplication::context() );
	const auto authority = context.callObjectMethod<jstring>( "getPackageName" ).toString() + QStringLiteral(".qtprovider");

	const auto file = QJniObject( "java/io/File", "(Ljava/lang/String;)V", QJniObject::fromString( path ).object<jstring>() );
	const auto uri = QJniObject::callStaticObjectMethod( "androidx/core/content/FileProvider", "getUriForFile",
														 "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
														 context.object(), QJniObject::fromString( authority ).object<jstring>(),
														 file.object() );
	if( uri.isValid() == false )
	{
		Q_EMIT notify( tr("File tidak bisa dibagikan"), QStringLiteral("error") );
		return;
	}

	auto intent = QJniObject( "android/content/Intent", "(Ljava/lang/String;)V",
							  QJniObject::fromString( QStringLiteral("android.intent.action.SEND") ).object<jstring>() );
	intent.callObjectMethod( "setType", "(Ljava/lang/String;)Landroid/content/Intent;",
							 QJniObject::fromString( QMimeDatabase().mimeTypeForFile( path ).name() ).object<jstring>() );
	intent.callObjectMethod( "putExtra", "(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;",
							 QJniObject::fromString( QStringLiteral("android.intent.extra.STREAM") ).object<jstring>(),
							 uri.object() );
	intent.callObjectMethod( "addFlags", "(I)Landroid/content/Intent;", 0x00000001 ); // FLAG_GRANT_READ_URI_PERMISSION

	const auto chooser = QJniObject::callStaticObjectMethod( "android/content/Intent", "createChooser",
															 "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
															 intent.object(),
															 QJniObject::fromString( tr("Bagikan") ).object<jstring>() );
	chooser.callObjectMethod( "addFlags", "(I)Landroid/content/Intent;", 0x10000000 ); // FLAG_ACTIVITY_NEW_TASK
	context.callMethod<void>( "startActivity", "(Landroid/content/Intent;)V", chooser.object() );
#else
	QDesktopServices::openUrl( QUrl::fromLocalFile( path ) );
#endif
}



QString MobileApp::devOption( const QString& name ) const
{
#ifdef Q_OS_ANDROID
	Q_UNUSED(name)
	return {};
#else
	return qEnvironmentVariable( name.toUtf8().constData() );
#endif
}



QStringList MobileApp::screenshots() const
{
	const QDir directory( screenshotDirectory() );

	QStringList files;
	const auto entries = directory.entryInfoList( { QStringLiteral("*.png") }, QDir::Files, QDir::Time );
	for( const auto& entry : entries )
	{
		files.append( entry.absoluteFilePath() );
	}
	return files;
}



bool MobileApp::deleteFile( const QString& path )
{
	// only ever touch our own screenshots and collected files
	const auto absolute = QFileInfo( path ).absoluteFilePath();
	if( absolute.startsWith( QDir( screenshotDirectory() ).absolutePath() + QLatin1Char('/') ) == false &&
		absolute.startsWith( QDir( collectedFilesDirectory() ).absolutePath() + QLatin1Char('/') ) == false )
	{
		return false;
	}
	return QFile::remove( path );
}



ComputerControlInterfaceList MobileApp::targets( const QStringList& uids ) const
{
	return uids.isEmpty() ? m_computers->visibleControlInterfaces() : m_computers->controlInterfaces( uids );
}



Feature MobileApp::featureByName( const QString& name ) const
{
	for( const auto& feature : VeyonCore::featureManager().features() )
	{
		if( feature.name() == name )
		{
			return feature;
		}
	}

	return Feature{};
}



void MobileApp::reconnectAll()
{
	// connections opened with the old credentials failed authentication -
	// restart them so the new ones are used right away
	for( const auto& controlInterface : m_master->computerControlListModel().computerControlInterfaces() )
	{
		const auto updateMode = controlInterface->updateMode();
		const auto size = controlInterface->scaledFramebufferSize();
		controlInterface->stop();
		controlInterface->start( size, updateMode );
	}
}



void MobileApp::checkAllLocations()
{
	auto treeModel = m_master->computerManager().computerTreeModel();
	for( int row = 0; row < treeModel->rowCount(); ++row )
	{
		const auto index = treeModel->index( row, 0 );
		if( treeModel->data( index, Qt::CheckStateRole ).value<Qt::CheckState>() != Qt::Checked )
		{
			treeModel->setData( index, Qt::Checked, Qt::CheckStateRole );
		}
	}
}



QJsonArray MobileApp::directoryObjects() const
{
	return VeyonCore::config().value( QStringLiteral("NetworkObjects"), QStringLiteral("BuiltinDirectory"), {} ).toJsonArray();
}



void MobileApp::setDirectoryObjects( const QJsonArray& objects )
{
	VeyonCore::config().setValue( QStringLiteral("NetworkObjects"), objects, QStringLiteral("BuiltinDirectory") );
	saveConfiguration();

	Q_EMIT roomsChanged();
	refreshComputers();
}



void MobileApp::saveConfiguration()
{
	VeyonCore::config().flushStore();
}



void MobileApp::handleUrl( const QUrl& url )
{
	// Qt for Android may call URL handlers from the Android UI thread
	if( QThread::currentThread() != thread() )
	{
		QMetaObject::invokeMethod( this, [this, url]() { handleUrl( url ); }, Qt::QueuedConnection );
		return;
	}

	if( url.scheme() != QStringLiteral("arunicontrol") )
	{
		return;
	}

	const auto error = m_gateways->addFromCode( url.toString() );
	if( error.isEmpty() )
	{
		Q_EMIT gatewayAdded( AruniTunnel::PairingInfo::decode( url.toString() ).siteName );
	}
	else
	{
		Q_EMIT notify( error, QStringLiteral("error") );
	}
}



QStringList MobileApp::managedLocationUids() const
{
	return mobileSettings().value( QStringLiteral("GatewayLocations") ).toStringList();
}



void MobileApp::publishGatewayHosts()
{
	// computers of remote sites appear as locations named after the site; they
	// are rewritten whenever a gateway reports its computers
	const auto previousLocations = managedLocationUids();

	QJsonArray objects;
	for( const auto& value : directoryObjects() )
	{
		const auto object = value.toObject();
		if( previousLocations.contains( object[QStringLiteral("Uid")].toString() ) == false &&
			previousLocations.contains( object[QStringLiteral("ParentUid")].toString() ) == false )
		{
			objects.append( object );
		}
	}

	static const auto ns = QUuid( QStringLiteral("{6b0e3c2a-94d1-4f7e-8b25-1d9a0c7e5f31}") );

	QStringList locations;
	const auto remote = m_gateways->remoteHosts();
	for( const auto& entry : remote )
	{
		const auto& site = entry.first;
		const auto locationUid = QUuid::createUuidV5( ns, QStringLiteral("site:") + site.gatewayId );
		locations.append( locationUid.toString() );

		objects.append( NetworkObject( NetworkObject::Type::Location, site.name, {}, {}, {}, locationUid ).toJson() );

		// laptops taken home get a room of their own
		const auto roamingUid = QUuid::createUuidV5( ns, QStringLiteral("roaming:") + site.gatewayId );
		bool haveRoaming = false;

		for( const auto& hostValue : entry.second )
		{
			const auto host = hostValue.toObject();
			const auto address = host[QStringLiteral("host")].toString();
			const bool roaming = host[QStringLiteral("roaming")].toBool();
			if( roaming && haveRoaming == false )
			{
				haveRoaming = true;
				locations.append( roamingUid.toString() );
				objects.append( NetworkObject( NetworkObject::Type::Location, tr("%1 · Di luar kantor").arg( site.name ),
											   {}, {}, {}, roamingUid ).toJson() );
			}
			objects.append( NetworkObject( NetworkObject::Type::Host,
										   host[QStringLiteral("name")].toString( address ),
										   address, host[QStringLiteral("mac")].toString(), {},
										   QUuid::createUuidV5( ns, site.gatewayId + QLatin1Char('/') + address ),
										   roaming ? roamingUid : locationUid ).toJson() );
		}
	}

	auto settings = mobileSettings();
	settings.setValue( QStringLiteral("GatewayLocations"), locations );

	if( objects != directoryObjects() )
	{
		setDirectoryObjects( objects );
	}
}
