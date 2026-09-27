/*
 * ExamModeFeaturePlugin.cpp - one-click exam mode
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
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

#include "ExamModeDialog.h"
#include "ExamModeFeaturePlugin.h"
#include "FeatureWorkerManager.h"
#include "Filesystem.h"
#include "KeyboardShortcutTrapper.h"
#include "PlatformInputDeviceFunctions.h"
#include "PlatformSessionFunctions.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


namespace {

constexpr int EnforceInterval = 20 * 1000;
constexpr int ResolveInterval = 10 * 60 * 1000;

// always reachable from computers in the office network
const auto LocalRanges = QStringLiteral("LocalSubnet,127.0.0.0/8,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16,"
										"169.254.0.0/16,224.0.0.0/4,fe80::/10,fc00::/7");

#if defined(Q_OS_WIN)
const auto RulePrefix = QStringLiteral("AruniControl-Exam-");
// the rule of the "Block internet" feature (plugins/internetaccess) - while
// it exists, internet stays blocked after the exam
const auto InternetBlockRule = QStringLiteral("AruniControl-NoInternet-LAN");

bool runNetsh( const QStringList& arguments )
{
	return QProcess::execute( QStringLiteral("netsh"), arguments ) == 0;
}

void addRule( const QString& name, const QStringList& options )
{
	runNetsh( QStringList{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
						   QStringLiteral("rule"), QStringLiteral("name=") + RulePrefix + name,
						   QStringLiteral("dir=out"), QStringLiteral("action=allow"), QStringLiteral("enable=yes") } + options );
}

void deleteRule( const QString& name )
{
	runNetsh( { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
				QStringLiteral("rule"), QStringLiteral("name=") + RulePrefix + name } );
}

bool ruleExists( const QString& fullName )
{
	return runNetsh( { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("show"),
					   QStringLiteral("rule"), QStringLiteral("name=") + fullName } );
}
#endif


QString normalizedDomain( const QString& text )
{
	auto value = text.trimmed().toLower();
	if( value.isEmpty() )
	{
		return {};
	}
	if( value.contains( QStringLiteral("://") ) == false )
	{
		value.prepend( QStringLiteral("http://") );
	}
	auto host = QUrl( value ).host();
	if( host.startsWith( QStringLiteral("www.") ) )
	{
		host.remove( 0, 4 );
	}
	return host.contains( QLatin1Char('.') ) ? host : QString{};
}



QStringList normalizedDomains( const QStringList& texts )
{
	QStringList domains;
	for( const auto& text : texts )
	{
		const auto domain = normalizedDomain( text );
		if( domain.isEmpty() == false && domains.contains( domain ) == false )
		{
			domains.append( domain );
		}
	}
	return domains;
}



QStringList cleanedApps( const QStringList& apps )
{
	QStringList result;
	for( auto app : apps )
	{
		app = app.trimmed();
		// only plain program names - they end up on a command line
		if( app.isEmpty() || app.contains( QRegularExpression( QStringLiteral("[\\\\/\"'&|<>;%$`]") ) ) )
		{
			continue;
		}
		if( app.endsWith( QStringLiteral(".exe"), Qt::CaseInsensitive ) )
		{
			app.chop( 4 );
		}
		if( result.contains( app, Qt::CaseInsensitive ) == false )
		{
			result.append( app );
		}
	}
	return result;
}

}



ExamModeFeaturePlugin::ExamModeFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_examModeFeature( Feature( QStringLiteral( "ExamMode" ),
								Feature::Flag::Mode | Feature::Flag::AllComponents,
								Feature::Uid( FeatureUid ),
								Feature::Uid(),
								tr( "Exam mode" ), tr( "End exam mode" ),
								tr( "Internet only for the exam website, other applications closed and system shortcuts "
									"locked - with one click, and undone with one click." ),
								QStringLiteral(":/exammode/exammode.png") ) ),
	m_features( { m_examModeFeature } )
{
	if( VeyonCore::component() == VeyonCore::Component::Server )
	{
		// keep the exam mode across a restart of the computer or the server
		QTimer::singleShot( 3000, this, [this]() {
			if( auto server = VeyonCore::instance()->findChild<VeyonServerInterface*>() )
			{
				initServer( *server );
			}
		} );
	}
}



const FeatureList& ExamModeFeaturePlugin::featureList() const
{
	return m_features;
}



QStringList ExamModeFeaturePlugin::defaultApps()
{
	return { QStringLiteral("WhatsApp"), QStringLiteral("Telegram"), QStringLiteral("Discord"), QStringLiteral("Line"),
			 QStringLiteral("Zoom"), QStringLiteral("ms-teams"), QStringLiteral("Teams"), QStringLiteral("Skype"),
			 QStringLiteral("AnyDesk"), QStringLiteral("TeamViewer"), QStringLiteral("Spotify"), QStringLiteral("steam"),
			 QStringLiteral("EpicGamesLauncher") };
}



QStringList ExamModeFeaturePlugin::requiredDomains()
{
	return { QStringLiteral("relay.arunihealth.id"), QStringLiteral("arunicontrol.arunihealth.id") };
}



bool ExamModeFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											const QVariantMap& arguments,
											const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_examModeFeature.uid() )
	{
		return false;
	}

	switch( operation )
	{
	case Operation::Start:
	{
		// never on the computer of the teacher itself
		auto examComputers = computerControlInterfaces;
		examComputers.removeLocalHostInterfaces();
		sendFeatureMessage( FeatureMessage{ featureUid, Start }
								.addArgument( Argument::Sites, normalizedDomains( arguments.value( QStringLiteral("sites") ).toStringList() ) )
								.addArgument( Argument::Apps, cleanedApps( arguments.value( QStringLiteral("apps"), defaultApps() ).toStringList() ) )
								.addArgument( Argument::LockKeys, arguments.value( QStringLiteral("lockKeys"), true ).toBool() )
								.addArgument( Argument::Url, arguments.value( QStringLiteral("url") ).toString().trimmed() )
								.addArgument( Argument::BlockInternet, arguments.value( QStringLiteral("blockInternet"), true ).toBool() ),
							examComputers );
		return true;
	}
	case Operation::Stop:
		sendFeatureMessage( FeatureMessage{ featureUid, Stop }, computerControlInterfaces );
		return true;
	case Operation::Initialize:
		sendFeatureMessage( FeatureMessage{ featureUid, Query }, computerControlInterfaces );
		return true;
	default:
		break;
	}

	return false;
}



bool ExamModeFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_examModeFeature.uid() || computerControlInterfaces.isEmpty() )
	{
		return false;
	}

	ExamModeDialog dialog( computerControlInterfaces.size(), master.mainWindow() );
	if( dialog.exec() != QDialog::Accepted )
	{
		return true;
	}

	controlFeature( feature.uid(), Operation::Start, dialog.arguments(), computerControlInterfaces );
	return true;
}



bool ExamModeFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												  const FeatureMessage& message )
{
	if( message.featureUid() != m_examModeFeature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Status )
	{
		Q_EMIT statusReceived( computerControlInterface, message.argument( Argument::Active ).toBool(),
							   message.argument( Argument::Error ).toString() );
	}
	return true;
}



bool ExamModeFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
												  const FeatureMessage& message )
{
	if( message.featureUid() != m_examModeFeature.uid() )
	{
		return false;
	}

	initServer( server );

	QString error;
	switch( static_cast<int>( message.command() ) )
	{
	case Start:
	{
		Settings settings;
		settings.active = true;
		settings.sites = normalizedDomains( message.argument( Argument::Sites ).toStringList() );
		settings.apps = cleanedApps( message.argument( Argument::Apps ).toStringList() );
		settings.lockKeys = message.argument( Argument::LockKeys ).toBool();
		settings.url = message.argument( Argument::Url ).toString();
		settings.blockInternet = message.argument( Argument::BlockInternet ).toBool();
		// the website opened for the exam is always reachable
		const auto urlDomain = normalizedDomain( settings.url );
		if( urlDomain.isEmpty() == false && settings.sites.contains( urlDomain ) == false )
		{
			settings.sites.append( urlDomain );
		}
		activate( server, settings, error );
		break;
	}
	case Stop:
		deactivate( server, error );
		break;
	default:
		error = m_lastError;
		break;
	}

	return server.sendFeatureMessageReply( messageContext,
		FeatureMessage{ m_examModeFeature.uid(), Status }
			.addArgument( Argument::Active, m_settings.active )
			.addArgument( Argument::Error, error ) );
}



bool ExamModeFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED(worker)

	if( message.featureUid() != m_examModeFeature.uid() )
	{
		return false;
	}

	switch( static_cast<int>( message.command() ) )
	{
	case StartWorker:
	{
		if( message.argument( Argument::LockKeys ).toBool() && m_trapper == nullptr )
		{
			// the Windows implementation swallows the shortcuts it traps
			auto trapper = VeyonCore::platform().inputDeviceFunctions().createKeyboardShortcutTrapper( this );
			if( trapper )
			{
				trapper->setEnabled( true );
				m_trapper = trapper;
			}
		}
		else if( message.argument( Argument::LockKeys ).toBool() == false && m_trapper )
		{
			delete m_trapper;
			m_trapper = nullptr;
		}

		const auto url = message.argument( Argument::Url ).toString();
		if( url.isEmpty() == false )
		{
			QDesktopServices::openUrl( QUrl::fromUserInput( url ) );
		}
		return true;
	}
	case StopWorker:
		delete m_trapper;
		m_trapper = nullptr;
		QCoreApplication::quit();
		return true;
	default:
		break;
	}

	return false;
}



bool ExamModeFeaturePlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

	return featureUid == m_examModeFeature.uid() && m_settings.active;
}



void ExamModeFeaturePlugin::initServer( VeyonServerInterface& server )
{
	if( m_serverInitialized )
	{
		return;
	}
	m_serverInitialized = true;

	connect( &m_enforceTimer, &QTimer::timeout, this, &ExamModeFeaturePlugin::closeApps );
	connect( &m_resolveTimer, &QTimer::timeout, this, &ExamModeFeaturePlugin::resolveSites );

	const auto settings = loadSettings();
	if( settings.active )
	{
		vInfo() << "exam mode was active - restoring it";
		QString error;
		// the running exam must not open the website again after a restart
		auto restored = settings;
		restored.url.clear();
		activate( server, restored, error );
	}
}



bool ExamModeFeaturePlugin::activate( VeyonServerInterface& server, const Settings& settings, QString& error )
{
	m_settings = settings;
	saveSettings( m_settings );

	closeApps();
	m_enforceTimer.start( EnforceInterval );

	setTaskManagerLocked( m_settings.lockKeys );

	bool ok = true;
	if( m_settings.blockInternet )
	{
		m_allowedAddresses.clear();
		// block right away, the exam websites follow once resolved
		ok = applyFirewall( true, error );
		resolveSites();
		m_resolveTimer.start( ResolveInterval );
	}
	else
	{
		m_resolveTimer.stop();
		applyFirewall( false, error );
	}

	startWorker( server );

	m_lastError = error;
	vInfo() << "exam mode started - sites:" << m_settings.sites << "apps:" << m_settings.apps
			<< "lock keys:" << m_settings.lockKeys << "block internet:" << m_settings.blockInternet;
	return ok;
}



void ExamModeFeaturePlugin::deactivate( VeyonServerInterface& server, QString& error )
{
	const bool wasActive = m_settings.active || loadSettings().active;

	m_enforceTimer.stop();
	m_resolveTimer.stop();
	m_settings = {};
	m_allowedAddresses.clear();
	saveSettings( m_settings );

	if( wasActive )
	{
		applyFirewall( false, error );
		setTaskManagerLocked( false );
	}

	if( server.featureWorkerManager().isWorkerRunning( m_examModeFeature.uid() ) )
	{
		server.featureWorkerManager().sendMessageToManagedSystemWorker( FeatureMessage{ m_examModeFeature.uid(), StopWorker } );
	}

	m_lastError = error;
	vInfo() << "exam mode ended";
}



void ExamModeFeaturePlugin::startWorker( VeyonServerInterface& server )
{
	if( VeyonCore::platform().sessionFunctions().currentSessionHasUser() == false )
	{
		return;
	}

	server.featureWorkerManager().sendMessageToManagedSystemWorker(
		FeatureMessage{ m_examModeFeature.uid(), StartWorker }
			.addArgument( Argument::LockKeys, m_settings.lockKeys )
			.addArgument( Argument::Url, m_settings.url ) );
}



bool ExamModeFeaturePlugin::stillActive()
{
	// another server instance of this computer (Windows: one per session)
	// may have ended the exam
	if( m_settings.active && loadSettings().active == false )
	{
		m_enforceTimer.stop();
		m_resolveTimer.stop();
		m_settings = {};
		m_allowedAddresses.clear();
	}
	return m_settings.active;
}



void ExamModeFeaturePlugin::closeApps()
{
	if( stillActive() == false || m_settings.apps.isEmpty() )
	{
		return;
	}

	for( const auto& app : std::as_const( m_settings.apps ) )
	{
#if defined(Q_OS_WIN)
		QProcess::startDetached( QStringLiteral("taskkill"), { QStringLiteral("/F"), QStringLiteral("/IM"),
															   app + QStringLiteral(".exe") } );
#elif defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
		QProcess::startDetached( QStringLiteral("/usr/bin/pkill"), { QStringLiteral("-i"), QStringLiteral("-x"), app } );
#else
		Q_UNUSED(app)
#endif
	}
}



void ExamModeFeaturePlugin::resolveSites()
{
	if( stillActive() == false || m_settings.blockInternet == false || m_pendingLookups > 0 )
	{
		return;
	}

	QStringList hosts;
	for( const auto& domain : m_settings.sites + requiredDomains() )
	{
		hosts.append( domain );
		if( domain.count( QLatin1Char('.') ) == 1 )
		{
			hosts.append( QStringLiteral("www.") + domain );
		}
	}

	m_pendingLookups = hosts.size();
	for( const auto& host : std::as_const( hosts ) )
	{
		QHostInfo::lookupHost( host, this, [this]( const QHostInfo& info ) {
			for( const auto& address : info.addresses() )
			{
				m_allowedAddresses.insert( address.toString() );
			}
			if( --m_pendingLookups == 0 && m_settings.active && m_settings.blockInternet )
			{
				QString error;
				applyFirewall( true, error );
				m_lastError = error;
			}
		} );
	}
}



bool ExamModeFeaturePlugin::applyFirewall( bool on, QString& error )
{
#if defined(Q_OS_WIN)
	// the service runs as LocalSystem - netsh needs no UAC prompt. All
	// outbound traffic is blocked except the office network, DHCP, DNS (so the
	// exam websites resolve) and the addresses of the exam websites.
	deleteRule( QStringLiteral("LAN") );
	deleteRule( QStringLiteral("DHCP") );
	deleteRule( QStringLiteral("DNS-UDP") );
	deleteRule( QStringLiteral("DNS-TCP") );
	deleteRule( QStringLiteral("Sites") );

	if( on )
	{
		addRule( QStringLiteral("LAN"), { QStringLiteral("remoteip=") + LocalRanges } );
		addRule( QStringLiteral("DHCP"), { QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=67,68") } );
		addRule( QStringLiteral("DNS-UDP"), { QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=53") } );
		addRule( QStringLiteral("DNS-TCP"), { QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=53") } );
		if( m_allowedAddresses.isEmpty() == false )
		{
			auto addresses = m_allowedAddresses.values();
			addresses.sort();
			addRule( QStringLiteral("Sites"), { QStringLiteral("remoteip=") + addresses.join( QLatin1Char(',') ) } );
		}

		if( runNetsh( { QStringLiteral("advfirewall"), QStringLiteral("set"), QStringLiteral("allprofiles"),
						QStringLiteral("firewallpolicy"), QStringLiteral("blockinbound,blockoutbound") } ) == false )
		{
			error = tr( "Could not change the Windows Firewall" );
			return false;
		}
		return true;
	}

	// internet stays blocked if the "Block internet" feature is on
	if( ruleExists( InternetBlockRule ) == false )
	{
		runNetsh( { QStringLiteral("advfirewall"), QStringLiteral("set"), QStringLiteral("allprofiles"),
					QStringLiteral("firewallpolicy"), QStringLiteral("blockinbound,allowoutbound") } );
	}
	return true;
#elif defined(Q_OS_MACOS)
	// the pf firewall needs administrator rights: the user of this Mac is asked
	QString script;
	if( on )
	{
		auto addresses = m_allowedAddresses.values();
		addresses.sort();
		const auto ruleset = QStringLiteral(
			"set block-policy drop\n"
			"table <aruni_local> persist { 127.0.0.0/8 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16 169.254.0.0/16 224.0.0.0/4 }\n"
			"table <aruni_exam> persist { %1 }\n"
			"block drop out all\n"
			"pass out quick to <aruni_local>\n"
			"pass out quick to <aruni_exam>\n"
			"pass out quick proto { udp tcp } from any to any port 53\n"
			"pass out quick proto udp from any to any port { 67, 68 }\n" ).arg( addresses.join( QLatin1Char(' ') ) );
		const auto rulesetPath = QDir::temp().filePath( QStringLiteral("aruni-exam.conf") );
		QFile file( rulesetPath );
		if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
		{
			error = tr( "Cannot prepare the firewall rules" );
			return false;
		}
		file.write( ruleset.toUtf8() );
		file.close();
		script = QStringLiteral("/sbin/pfctl -E -f '%1'").arg( rulesetPath );
	}
	else
	{
		script = QStringLiteral("/sbin/pfctl -f /etc/pf.conf");
	}

	// asynchronous: the server keeps running while the password dialog is open
	auto process = new QProcess( this );
	connect( process, &QProcess::finished, process, [process]( int exitCode ) {
		if( exitCode != 0 )
		{
			vWarning() << "exam mode firewall change was not approved:"
					   << QString::fromUtf8( process->readAllStandardError() ).trimmed();
		}
		process->deleteLater();
	} );
	process->start( QStringLiteral("/usr/bin/osascript"),
					{ QStringLiteral("-e"), QStringLiteral("do shell script \"%1\" with administrator privileges").arg( script ) } );
	if( on )
	{
		error = tr( "Waiting for the administrator password on this Mac" );
	}
	return true;
#else
	Q_UNUSED(on)
	error = tr( "Blocking the internet is not supported on this operating system" );
	return false;
#endif
}



void ExamModeFeaturePlugin::setTaskManagerLocked( bool locked )
{
#if defined(Q_OS_WIN)
	// machine-wide policy, applies to every user of the computer
	QSettings policy( QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"),
					  QSettings::NativeFormat );
	if( locked )
	{
		policy.setValue( QStringLiteral("DisableTaskMgr"), 1 );
	}
	else
	{
		policy.remove( QStringLiteral("DisableTaskMgr") );
	}
#else
	Q_UNUSED(locked)
#endif
}



QString ExamModeFeaturePlugin::statePath()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/exammode.json") );
}



ExamModeFeaturePlugin::Settings ExamModeFeaturePlugin::loadSettings()
{
	Settings settings;
	QFile file( statePath() );
	if( file.open( QFile::ReadOnly ) )
	{
		const auto json = QJsonDocument::fromJson( file.readAll() ).object();
		settings.active = json[QStringLiteral("active")].toBool();
		settings.sites = json[QStringLiteral("sites")].toVariant().toStringList();
		settings.apps = json[QStringLiteral("apps")].toVariant().toStringList();
		settings.lockKeys = json[QStringLiteral("lockKeys")].toBool( true );
		settings.url = json[QStringLiteral("url")].toString();
		settings.blockInternet = json[QStringLiteral("blockInternet")].toBool( true );
	}
	return settings;
}



void ExamModeFeaturePlugin::saveSettings( const Settings& settings )
{
	QDir().mkpath( QFileInfo( statePath() ).absolutePath() );
	QFile file( statePath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( QJsonObject{
			{ QStringLiteral("active"), settings.active },
			{ QStringLiteral("sites"), QJsonArray::fromStringList( settings.sites ) },
			{ QStringLiteral("apps"), QJsonArray::fromStringList( settings.apps ) },
			{ QStringLiteral("lockKeys"), settings.lockKeys },
			{ QStringLiteral("url"), settings.url },
			{ QStringLiteral("blockInternet"), settings.blockInternet },
		} ).toJson() );
	}
}
