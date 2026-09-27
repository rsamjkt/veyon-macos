/*
 * InventoryFeaturePlugin.cpp - hardware, disks and software of a computer
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

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QProcess>
#include <QSettings>
#include <QStorageInfo>
#include <QSysInfo>
#include <QThread>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#include <sys/time.h>
#include <ctime>
#endif

#include "InstalledSoftware.h"
#include "InventoryFeaturePlugin.h"
#include "PlatformUserFunctions.h"
#include "VeyonCore.h"
#include "VeyonServerInterface.h"


namespace {

#if defined(Q_OS_MACOS)
QString sysctlString( const char* name )
{
	size_t size = 0;
	if( sysctlbyname( name, nullptr, &size, nullptr, 0 ) != 0 || size == 0 )
	{
		return {};
	}
	QByteArray buffer( int( size ), 0 );
	if( sysctlbyname( name, buffer.data(), &size, nullptr, 0 ) != 0 )
	{
		return {};
	}
	return QString::fromUtf8( buffer.constData() ).trimmed();
}
#endif

constexpr double GiB = 1024.0 * 1024.0 * 1024.0;

double roundedGB( qint64 bytes )
{
	return qRound( bytes / GiB * 10 ) / 10.0;
}

}



InventoryFeaturePlugin::InventoryFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_feature( Feature( QStringLiteral( "Inventory" ),
						Feature::Flag::Meta | Feature::Flag::Service,
						Feature::Uid( FeatureUid ),
						Feature::Uid(),
						tr( "Inventory" ), {},
						tr( "Hardware, disks and software of the computer" ) ) ),
	m_features( { m_feature } )
{
}



const FeatureList& InventoryFeaturePlugin::featureList() const
{
	return m_features;
}



bool InventoryFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
											 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( featureUid != m_feature.uid() || operation != Operation::Start )
	{
		return false;
	}

	sendFeatureMessage( FeatureMessage{ featureUid, Query }, computerControlInterfaces );
	return true;
}



bool InventoryFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												   const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Info )
	{
		Q_EMIT inventoryReceived( computerControlInterface,
								  QJsonDocument::fromJson( message.argument( Argument::Inventory ).toByteArray() ).object() );
	}
	return true;
}



bool InventoryFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
												   const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	querySerialNumber();

	return server.sendFeatureMessageReply( messageContext,
		FeatureMessage{ m_feature.uid(), Info }
			.addArgument( Argument::Inventory, QJsonDocument( collect() ).toJson( QJsonDocument::Compact ) ) );
}



QJsonObject InventoryFeaturePlugin::collect()
{
	QJsonObject info{
		{ QStringLiteral("host"), QHostInfo::localHostName() },
		{ QStringLiteral("os"), QSysInfo::prettyProductName() },
		{ QStringLiteral("kernel"), QSysInfo::kernelVersion() },
		{ QStringLiteral("arch"), QSysInfo::currentCpuArchitecture() },
		{ QStringLiteral("cores"), QThread::idealThreadCount() },
		{ QStringLiteral("version"), VeyonCore::versionString() },
		{ QStringLiteral("user"), VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName ) },
		{ QStringLiteral("serial"), m_serialNumber },
	};

#if defined(Q_OS_WIN)
	const QSettings cpu( QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), QSettings::NativeFormat );
	info[QStringLiteral("cpu")] = cpu.value( QStringLiteral("ProcessorNameString") ).toString().simplified();
	const QSettings bios( QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS"), QSettings::NativeFormat );
	info[QStringLiteral("manufacturer")] = bios.value( QStringLiteral("SystemManufacturer") ).toString().simplified();
	info[QStringLiteral("model")] = bios.value( QStringLiteral("SystemProductName") ).toString().simplified();

	MEMORYSTATUSEX memory;
	memory.dwLength = sizeof( memory );
	if( GlobalMemoryStatusEx( &memory ) )
	{
		info[QStringLiteral("ramMB")] = qint64( memory.ullTotalPhys / ( 1024 * 1024 ) );
	}
	info[QStringLiteral("uptimeHours")] = qint64( GetTickCount64() / ( 3600 * 1000 ) );
#elif defined(Q_OS_MACOS)
	info[QStringLiteral("cpu")] = sysctlString( "machdep.cpu.brand_string" );
	info[QStringLiteral("manufacturer")] = QStringLiteral("Apple");
	info[QStringLiteral("model")] = sysctlString( "hw.model" );
	uint64_t memory = 0;
	size_t size = sizeof( memory );
	if( sysctlbyname( "hw.memsize", &memory, &size, nullptr, 0 ) == 0 )
	{
		info[QStringLiteral("ramMB")] = qint64( memory / ( 1024 * 1024 ) );
	}
	struct timeval boot{};
	size = sizeof( boot );
	if( sysctlbyname( "kern.boottime", &boot, &size, nullptr, 0 ) == 0 && boot.tv_sec > 0 )
	{
		info[QStringLiteral("uptimeHours")] = qint64( ( std::time( nullptr ) - boot.tv_sec ) / 3600 );
	}
#endif

	QJsonArray disks;
	const auto volumes = QStorageInfo::mountedVolumes();
	for( const auto& volume : volumes )
	{
		const auto fileSystem = QString::fromLatin1( volume.fileSystemType() ).toLower();
		if( volume.isValid() == false || volume.isReady() == false || volume.bytesTotal() < qint64( GiB ) ||
			fileSystem.contains( QStringLiteral("tmpfs") ) || fileSystem == QStringLiteral("devfs") ||
			fileSystem == QStringLiteral("autofs") ||
			// the macOS system volumes besides the data volume
			volume.rootPath().startsWith( QStringLiteral("/System/Volumes/") ) ||
			volume.rootPath().startsWith( QStringLiteral("/private/var/vm") ) )
		{
			continue;
		}
		disks.append( QJsonObject{
			{ QStringLiteral("name"), volume.displayName() },
			{ QStringLiteral("path"), volume.rootPath() },
			{ QStringLiteral("totalGB"), roundedGB( volume.bytesTotal() ) },
			{ QStringLiteral("freeGB"), roundedGB( volume.bytesAvailable() ) },
		} );
	}
	info[QStringLiteral("disks")] = disks;

	QJsonArray network;
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& networkInterface : interfaces )
	{
		if( networkInterface.type() == QNetworkInterface::Loopback ||
			( networkInterface.flags() & QNetworkInterface::IsUp ) == 0 ||
			networkInterface.hardwareAddress().isEmpty() )
		{
			continue;
		}
		QStringList addresses;
		const auto entries = networkInterface.addressEntries();
		for( const auto& entry : entries )
		{
			if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol )
			{
				addresses.append( entry.ip().toString() );
			}
		}
		if( addresses.isEmpty() )
		{
			continue;
		}
		network.append( QJsonObject{
			{ QStringLiteral("name"), networkInterface.humanReadableName() },
			{ QStringLiteral("mac"), networkInterface.hardwareAddress() },
			{ QStringLiteral("ip"), addresses.join( QStringLiteral(", ") ) },
		} );
	}
	info[QStringLiteral("network")] = network;

	info[QStringLiteral("software")] = InstalledSoftware::toJson( InstalledSoftware::list() );

	return info;
}



void InventoryFeaturePlugin::querySerialNumber()
{
	if( m_serialQueried )
	{
		return;
	}
	m_serialQueried = true;

	// takes a moment - the next query gets it
	auto process = new QProcess( this );
	connect( process, &QProcess::finished, this, [this, process]() {
		auto output = QString::fromUtf8( process->readAllStandardOutput() );
#if defined(Q_OS_MACOS)
		const auto start = output.indexOf( QStringLiteral("\"IOPlatformSerialNumber\" = \"") );
		output = start < 0 ? QString{} : output.mid( start + 28 ).section( QLatin1Char('"'), 0, 0 );
#endif
		m_serialNumber = output.trimmed();
		process->deleteLater();
	} );
#if defined(Q_OS_WIN)
	process->start( QStringLiteral("powershell"), { QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
													QStringLiteral("-Command"), QStringLiteral("(Get-CimInstance Win32_BIOS).SerialNumber") } );
#elif defined(Q_OS_MACOS)
	process->start( QStringLiteral("/usr/sbin/ioreg"), { QStringLiteral("-rd1"), QStringLiteral("-c"), QStringLiteral("IOPlatformExpertDevice") } );
#else
	delete process;
#endif
}
