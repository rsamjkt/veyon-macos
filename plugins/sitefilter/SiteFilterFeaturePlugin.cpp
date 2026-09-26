/*
 * SiteFilterFeaturePlugin.cpp - block websites on selected computers
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryFile>
#include <QUrl>

#include "Filesystem.h"
#include "FeatureWorkerManager.h"
#include "PlatformCoreFunctions.h"
#include "SiteFilterDialog.h"
#include "SiteFilterFeaturePlugin.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


namespace {

constexpr auto BeginMarker = "# BEGIN AruniControl blocked sites - managed automatically, do not edit";
constexpr auto EndMarker = "# END AruniControl blocked sites";

// hosts cannot block wildcard subdomains - cover the usual ones
const QStringList CommonPrefixes{ QString{}, QStringLiteral("www."), QStringLiteral("m."), QStringLiteral("mobile.") };

QString hostsPath()
{
#if defined(Q_OS_WIN)
	const auto systemRoot = qEnvironmentVariable( "SystemRoot", QStringLiteral("C:\\Windows") );
	return QDir( systemRoot ).filePath( QStringLiteral("System32/drivers/etc/hosts") );
#else
	return QStringLiteral("/etc/hosts");
#endif
}



// hosts file content with our block replaced by one for the given sites
QByteArray hostsWithSites( const QByteArray& current, const QStringList& sites )
{
	QList<QByteArray> kept;
	bool inBlock = false;
	const auto lines = current.split( '\n' );
	for( const auto& line : lines )
	{
		const auto trimmed = line.trimmed();
		if( trimmed.startsWith( "# BEGIN AruniControl" ) )
		{
			inBlock = true;
			continue;
		}
		if( trimmed.startsWith( "# END AruniControl" ) )
		{
			inBlock = false;
			continue;
		}
		if( inBlock == false )
		{
			kept.append( line );
		}
	}

	// no trailing empty lines before our block
	while( kept.isEmpty() == false && kept.last().trimmed().isEmpty() )
	{
		kept.removeLast();
	}

	auto result = kept.join( '\n' );
	const auto newline = current.contains( "\r\n" ) ? QByteArray( "\r\n" ) : QByteArray( "\n" );
	result.replace( "\r\n", "\n" );
	result.replace( "\n", newline );

	if( sites.isEmpty() == false )
	{
		result += newline + newline + BeginMarker + newline;
		for( const auto& site : sites )
		{
			for( const auto& prefix : CommonPrefixes )
			{
				const auto host = ( prefix + site ).toLatin1();
				result += "0.0.0.0 " + host + newline;
				result += "::0 " + host + newline;
			}
		}
		result += EndMarker;
	}
	result += newline;
	return result;
}

}



SiteFilterFeaturePlugin::SiteFilterFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_siteFilterFeature( Feature( QStringLiteral( "SiteFilter" ),
								  Feature::Flag::Action | Feature::Flag::AllComponents,
								  Feature::Uid( FeatureUid ),
								  Feature::Uid(),
								  tr( "Block websites" ), {},
								  tr( "Block selected websites (e.g. social media or games) on the selected computers." ),
								  QStringLiteral(":/sitefilter/sitefilter.png") ) ),
	m_features( { m_siteFilterFeature } )
{
}



const FeatureList& SiteFilterFeaturePlugin::featureList() const
{
	return m_features;
}



QString SiteFilterFeaturePlugin::normalizedDomain( const QString& text )
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

	static const QRegularExpression domainRX{ QStringLiteral("^([a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?\\.)+[a-z]{2,63}$") };
	return domainRX.match( host ).hasMatch() ? host : QString{};
}



QStringList SiteFilterFeaturePlugin::normalizedDomains( const QStringList& texts )
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



QList<QPair<QString, QStringList>> SiteFilterFeaturePlugin::presets()
{
	return {
		{ tr( "Social media" ), { QStringLiteral("facebook.com"), QStringLiteral("instagram.com"), QStringLiteral("tiktok.com"),
								  QStringLiteral("x.com"), QStringLiteral("twitter.com"), QStringLiteral("threads.net"),
								  QStringLiteral("snapchat.com") } },
		{ tr( "Video" ), { QStringLiteral("youtube.com"), QStringLiteral("youtu.be"), QStringLiteral("netflix.com"),
						   QStringLiteral("vidio.com"), QStringLiteral("twitch.tv") } },
		{ tr( "Online games" ), { QStringLiteral("roblox.com"), QStringLiteral("steampowered.com"), QStringLiteral("epicgames.com"),
								  QStringLiteral("miniclip.com"), QStringLiteral("poki.com"), QStringLiteral("friv.com") } },
		{ tr( "Chat" ), { QStringLiteral("web.whatsapp.com"), QStringLiteral("web.telegram.org"), QStringLiteral("discord.com") } },
	};
}



QStringList SiteFilterFeaturePlugin::sitesOf( const ComputerControlInterface::Pointer& computerControlInterface ) const
{
	return m_reportedSites.value( computerControlInterface.data() );
}



bool SiteFilterFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											  const QVariantMap& arguments,
											  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_siteFilterFeature.uid() )
	{
		return false;
	}

	switch( operation )
	{
	case Operation::Start:
		sendFeatureMessage( FeatureMessage{ featureUid, SetSites }
								.addArgument( Argument::Sites, normalizedDomains( arguments.value( QStringLiteral("sites") ).toStringList() ) ),
							computerControlInterfaces );
		return true;
	case Operation::Stop:
		sendFeatureMessage( FeatureMessage{ featureUid, SetSites }.addArgument( Argument::Sites, QStringList{} ),
							computerControlInterfaces );
		return true;
	case Operation::Initialize:
		sendFeatureMessage( FeatureMessage{ featureUid, Query }, computerControlInterfaces );
		return true;
	default:
		break;
	}

	return false;
}



bool SiteFilterFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
											const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_siteFilterFeature.uid() || computerControlInterfaces.isEmpty() )
	{
		return false;
	}

	// ask the computers for their current lists - the dialog shows them
	controlFeature( feature.uid(), Operation::Initialize, {}, computerControlInterfaces );

	SiteFilterDialog dialog( this, computerControlInterfaces, master.mainWindow() );
	if( dialog.exec() == QDialog::Accepted )
	{
		controlFeature( feature.uid(), dialog.sites().isEmpty() ? Operation::Stop : Operation::Start,
						{ { QStringLiteral("sites"), dialog.sites() } }, computerControlInterfaces );
	}

	return true;
}



bool SiteFilterFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
													const FeatureMessage& message )
{
	if( message.featureUid() != m_siteFilterFeature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Status )
	{
		const auto sites = message.argument( Argument::Sites ).toStringList();
		m_reportedSites[computerControlInterface.data()] = sites;
		Q_EMIT statusReceived( computerControlInterface, sites, message.argument( Argument::Supported ).toBool(),
							   message.argument( Argument::Error ).toString() );
	}
	return true;
}



bool SiteFilterFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
													const FeatureMessage& message )
{
	if( message.featureUid() != m_siteFilterFeature.uid() )
	{
		return false;
	}

	QString error;
	bool supported = true;
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
	supported = false;
	error = tr( "Not supported on this operating system" );
#endif

	if( static_cast<int>( message.command() ) == SetSites && supported )
	{
		const auto sites = normalizedDomains( message.argument( Argument::Sites ).toStringList() );
		if( applySites( sites, error ) )
		{
#if !defined(Q_OS_MACOS)
			// on macOS saved once the administrator approved (applySites())
			saveSites( sites );
#endif
			vInfo() << "blocked websites:" << sites;
		}
	}

	return server.sendFeatureMessageReply( messageContext,
		FeatureMessage{ m_siteFilterFeature.uid(), Status }
			.addArgument( Argument::Sites, loadSites() )
			.addArgument( Argument::Supported, supported )
			.addArgument( Argument::Error, error ) );
}



bool SiteFilterFeaturePlugin::applySites( const QStringList& sites, QString& error )
{
	QFile hosts( hostsPath() );
	if( hosts.open( QFile::ReadOnly ) == false )
	{
		error = tr( "Cannot read %1" ).arg( hostsPath() );
		return false;
	}
	const auto content = hostsWithSites( hosts.readAll(), sites );
	hosts.close();

#if defined(Q_OS_WIN)
	// the service runs as LocalSystem, so the hosts file is writable
	if( hosts.open( QFile::WriteOnly | QFile::Truncate ) == false || hosts.write( content ) != content.size() )
	{
		error = tr( "Cannot write %1 (blocked by antivirus?)" ).arg( hostsPath() );
		return false;
	}
	hosts.close();

	// browsers resolving through DNS over HTTPS would ignore the hosts file
	const bool blocking = sites.isEmpty() == false;
	for( const auto& key : { QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Google\\Chrome"),
							 QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Edge") } )
	{
		QSettings policy( key, QSettings::NativeFormat );
		if( blocking )
		{
			policy.setValue( QStringLiteral("DnsOverHttpsMode"), QStringLiteral("off") );
		}
		else
		{
			policy.remove( QStringLiteral("DnsOverHttpsMode") );
		}
	}
	QSettings firefox( QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Mozilla\\Firefox\\DNSOverHTTPS"),
					   QSettings::NativeFormat );
	if( blocking )
	{
		firefox.setValue( QStringLiteral("Enabled"), 0 );
		firefox.setValue( QStringLiteral("Locked"), 1 );
	}
	else
	{
		firefox.remove( QStringLiteral("Enabled") );
		firefox.remove( QStringLiteral("Locked") );
	}

	QProcess::execute( QStringLiteral("ipconfig"), { QStringLiteral("/flushdns") } );
	return true;
#elif defined(Q_OS_MACOS)
	// /etc/hosts belongs to root - the user of this Mac is asked for the
	// administrator password once per change
	QTemporaryFile temp( QDir::temp().filePath( QStringLiteral("aruni-hosts-XXXXXX") ) );
	temp.setAutoRemove( false );
	if( temp.open() == false || temp.write( content ) != content.size() )
	{
		error = tr( "Cannot prepare the hosts file" );
		return false;
	}
	temp.close();

	// asynchronous: the server must keep running while the password dialog is
	// open (osascript waits for it)
	const auto script = QStringLiteral("/bin/cat '%1' > /etc/hosts && /usr/bin/dscacheutil -flushcache && "
									   "/usr/bin/killall -HUP mDNSResponder").arg( temp.fileName() );
	auto process = new QProcess( this );
	connect( process, &QProcess::finished, this, [process, sites, file = temp.fileName()]( int exitCode ) {
		QFile::remove( file );
		process->deleteLater();
		if( exitCode == 0 )
		{
			saveSites( sites );
		}
		else
		{
			vWarning() << "blocking websites was not approved by an administrator:"
					   << QString::fromUtf8( process->readAllStandardError() ).trimmed();
		}
	} );
	process->start( QStringLiteral("/usr/bin/osascript"),
					{ QStringLiteral("-e"), QStringLiteral("do shell script \"%1\" with administrator privileges").arg( script ) } );
	error = tr( "Waiting for the administrator password on this Mac" );
	return true;
#else
	Q_UNUSED(content)
	error = tr( "Not supported on this operating system" );
	return false;
#endif
}



QString SiteFilterFeaturePlugin::statePath()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/sitefilter.json") );
}



QStringList SiteFilterFeaturePlugin::loadSites()
{
	QFile file( statePath() );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	return QJsonDocument::fromJson( file.readAll() ).object()[QStringLiteral("sites")].toVariant().toStringList();
}



void SiteFilterFeaturePlugin::saveSites( const QStringList& sites )
{
	QDir().mkpath( QFileInfo( statePath() ).absolutePath() );
	QFile file( statePath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( QJsonObject{ { QStringLiteral("sites"), QJsonArray::fromStringList( sites ) } } ).toJson() );
	}
}
