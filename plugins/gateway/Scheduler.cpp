/*
 * Scheduler.cpp - runs the schedules on the computers
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
#include <QHostInfo>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QUuid>

#include "ActivityLog.h"
#include "AdminRoles.h"
#include "AuthenticationCredentials.h"
#include "FeatureManager.h"
#include "Filesystem.h"
#include "GatewayState.h"
#include "MonitoringCollector.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "Notifier.h"
#include "Scheduler.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


namespace {

const auto BuiltinDirectoryPluginUid = Plugin::Uid( QStringLiteral("14bacaaa-ebe5-449c-b881-5b382f952571") );

// features started by the schedules (see the respective plugins)
const auto ScheduleRebootUid = Feature::Uid( QStringLiteral("4f7d98f0-395a-4fff-b968-e49b8d0f748c") );
const auto SchedulePowerDownDelayedUid = Feature::Uid( QStringLiteral("352de795-7fc4-4850-bc57-525bcb7033f5") );
const auto ScheduleScreenLockUid = Feature::Uid( QStringLiteral("ccb535a2-1d24-4cc1-a709-8b47d2b2ac79") );
const auto ScheduleInternetUid = Feature::Uid( QStringLiteral("6f3a1d27-9b40-4c85-a216-0e8d5f7b3c91") );
const auto ScheduleSiteFilterUid = Feature::Uid( QStringLiteral("c3a5e2d1-7b4f-4e8a-9d61-2f0b8e7c4a15") );
const auto ScheduleTextMessageUid = Feature::Uid( QStringLiteral("e75ae9c8-ac17-4d00-8f0d-019348346208") );
const auto ScheduleExamModeUid = Feature::Uid( QStringLiteral("9b2e6c41-8d7a-4f35-a0c9-4e1b7d3f5a28") );
const auto ScheduleDeviceControlUid = Feature::Uid( QStringLiteral("a6d31b7e-9f24-4c85-b0e6-3e8c5a1f7d29") );
const auto ScheduleAdminRolesUid = Feature::Uid( QStringLiteral("2f8b6d14-9c3e-4a57-b0e1-5d7a9c4f8e23") );

// users get this long to save their work before the computers power down
constexpr int PowerDownWarningSeconds = 120;

// the feature messages need a moment to leave before the connection closes
constexpr int SendDelay = 3000;

constexpr auto ScheduleAnnouncedName = "Aruni Gateway";


bool isThisComputer( const QString& host )
{
	if( host.compare( QHostInfo::localHostName(), Qt::CaseInsensitive ) == 0 ||
		host.compare( QStringLiteral("localhost"), Qt::CaseInsensitive ) == 0 )
	{
		return true;
	}

	const QHostAddress address( host );
	if( address.isNull() )
	{
		return false;
	}
	if( address.isLoopback() )
	{
		return true;
	}
	const auto addresses = QNetworkInterface::allAddresses();
	return std::any_of( addresses.cbegin(), addresses.cend(), [&address]( const QHostAddress& local ) {
		return local.isEqual( address, QHostAddress::ConvertV4MappedToIPv4 );
	} );
}

}



Scheduler::Scheduler( QObject* parent ) :
	QObject( parent ),
	m_lock( QDir( GatewayState::directory() ).filePath( QStringLiteral("scheduler.lock") ) )
{
	m_lock.setStaleLockTime( 0 );
	m_notifier = new Notifier( this );

	connect( &m_timer, &QTimer::timeout, this, &Scheduler::tick );
	m_timer.start( TickInterval );
	QTimer::singleShot( 5000, this, &Scheduler::tick );
}



Scheduler::~Scheduler()
{
	for( auto& visit : m_visits )
	{
		visit.control->stop();
	}
	m_visits.clear();
}



void Scheduler::tick()
{
	const auto schedules = Schedules::load();
	const auto runRequests = Schedules::takeRunRequests();
	if( schedules.rules.isEmpty() && runRequests.isEmpty() )
	{
		return;
	}

	// only one server instance of this computer runs the schedules
	if( m_haveLock == false )
	{
		QDir().mkpath( GatewayState::directory() );
		m_haveLock = m_lock.tryLock( 0 );
		if( m_haveLock == false )
		{
			// the other instance takes the requests - hand them back
			for( const auto& id : runRequests )
			{
				Schedules::requestRun( id );
			}
			return;
		}
	}

	const auto now = QDateTime::currentDateTime();
	const auto today = now.date().toString( Qt::ISODate );
	auto status = Schedules::readStatus();

	if( runRequests.contains( QString::fromLatin1( Schedules::PushRolesId ) ) )
	{
		Schedules::Rule rule;
		rule.id = QString::fromLatin1( Schedules::PushRolesId );
		rule.name = Schedules::actionName( Schedules::Action::PushRoles );
		rule.action = Schedules::Action::PushRoles;
		startRun( rule, true );
	}

	for( const auto& rule : schedules.rules )
	{
		if( runRequests.contains( rule.id ) )
		{
			startRun( rule, true );
			continue;
		}

		if( rule.enabled == false || rule.runsOn( now.date() ) == false ||
			status[rule.id].toObject()[QStringLiteral("date")].toString() == today )
		{
			continue;
		}

		// a little late is fine (the computer was busy or just started), a
		// schedule missed by far is skipped - powering down at 9 in the
		// morning because the admin computer was off at 17:00 would be wrong
		const auto due = QDateTime( now.date(), rule.time );
		const auto late = due.secsTo( now );
		if( late >= 0 && late <= LateStartSeconds )
		{
			auto entry = status[rule.id].toObject();
			entry[QStringLiteral("date")] = today;
			status[rule.id] = entry;
			Schedules::writeStatus( status );
			startRun( rule, false );
		}
	}
}



void Scheduler::startRun( const Schedules::Rule& rule, bool manual )
{
	for( const auto& run : std::as_const( m_runs ) )
	{
		if( run.rule.id == rule.id )
		{
			return;
		}
	}

	vInfo() << "schedule" << rule.name << ( manual ? "(run now)" : "" ) << Schedules::actionKey( rule.action );

	Run run;
	run.rule = rule;
	run.pending = targets( rule.room );
	run.total = run.pending.size();

	const auto runId = QUuid::createUuid().toString();

	if( rule.action == Schedules::Action::PowerOn )
	{
		for( const auto& target : std::as_const( run.pending ) )
		{
			if( target.mac.isEmpty() )
			{
				run.failed.append( target.name );
			}
			else
			{
				wakeOnLan( target.mac );
				++run.succeeded;
			}
		}
		// computers on a busy network sometimes miss the first packet
		QTimer::singleShot( 30000, this, [all = run.pending]() {
			for( const auto& target : all )
			{
				wakeOnLan( target.mac );
			}
		} );
		run.pending.clear();
		m_runs.insert( runId, run );
		finishRun( runId );
		return;
	}

	if( run.pending.isEmpty() == false && loadCredentials() == false )
	{
		run.failed.append( tr( "no private authentication key on this computer" ) );
		run.pending.clear();
	}

	m_runs.insert( runId, run );
	continueRun( runId );
}



void Scheduler::continueRun( const QString& runId )
{
	auto it = m_runs.find( runId );
	if( it == m_runs.end() )
	{
		return;
	}

	while( it->pending.isEmpty() == false && m_visits.size() < MaxParallelVisits )
	{
		const auto target = it->pending.takeFirst();
		++it->active;
		visit( runId, target );
		it = m_runs.find( runId );
	}

	if( it->pending.isEmpty() && it->active == 0 )
	{
		finishRun( runId );
	}
}



void Scheduler::visit( const QString& runId, const Target& target )
{
	const Computer computer( QUuid::createUuid(), target.name, target.host );
	auto control = ComputerControlInterface::Pointer::create( computer );

	Visit visit;
	visit.control = control;
	visit.runId = runId;
	visit.target = target;
	visit.timer = new QTimer( this );
	visit.timer->setSingleShot( true );
	const auto key = control.data();
	connect( visit.timer, &QTimer::timeout, this, [this, key]() {
		auto it = m_visits.find( key );
		if( it != m_visits.end() )
		{
			finishVisit( key, it->sent );
		}
	} );
	visit.timer->start( VisitTimeout );

	connect( control.data(), &ComputerControlInterface::stateChanged, this, [this, key]() {
		auto it = m_visits.find( key );
		if( it == m_visits.end() || it->sent )
		{
			return;
		}

		const auto state = it->control->state();
		if( state == ComputerControlInterface::State::AuthenticationFailed ||
			state == ComputerControlInterface::State::AccessControlFailed )
		{
			finishVisit( key, false );
		}
		else if( state == ComputerControlInterface::State::Connected )
		{
			const auto run = m_runs.value( it->runId );
			if( sendAction( run.rule, it->control ) == false )
			{
				finishVisit( key, false );
				return;
			}
			it->sent = true;
			// the message is on its way - close the connection a bit later
			it->timer->start( SendDelay );
		}
	} );

	m_visits.insert( key, visit );
	control->start( {}, ComputerControlInterface::UpdateMode::FeatureControlOnly );
}



void Scheduler::finishVisit( ComputerControlInterface* control, bool ok )
{
	auto visit = m_visits.take( control );
	if( visit.control.isNull() )
	{
		return;
	}

	visit.timer->deleteLater();
	visit.control->stop();

	auto it = m_runs.find( visit.runId );
	if( it != m_runs.end() )
	{
		--it->active;
		if( ok )
		{
			++it->succeeded;
		}
		else
		{
			it->failed.append( visit.target.name );
		}
	}

	// the control interface must not be destroyed inside its own signal
	QTimer::singleShot( 0, this, [control = visit.control]() { Q_UNUSED(control) } );

	// free slots go to any run still waiting
	const auto runIds = m_runs.keys();
	for( const auto& runId : runIds )
	{
		continueRun( runId );
	}
}



void Scheduler::finishRun( const QString& runId )
{
	const auto run = m_runs.take( runId );
	if( run.rule.id.isEmpty() )
	{
		return;
	}

	QString text;
	if( run.total == 0 )
	{
		text = run.rule.room.isEmpty() ? tr( "No computers configured" ) : tr( "No computers in room %1" ).arg( run.rule.room );
	}
	else
	{
		text = tr( "%1 of %2 computers" ).arg( run.succeeded ).arg( run.total );
		if( run.failed.isEmpty() == false )
		{
			auto failed = run.failed.mid( 0, 10 );
			if( run.failed.size() > failed.size() )
			{
				failed.append( QStringLiteral("…") );
			}
			text += QStringLiteral(" - ") + tr( "not reached: %1" ).arg( failed.join( QStringLiteral(", ") ) );
		}
	}

	vInfo() << "schedule" << run.rule.name << "done:" << text;

	auto status = Schedules::readStatus();
	auto entry = status[run.rule.id].toObject();
	entry[QStringLiteral("time")] = QDateTime::currentDateTime().toString( Qt::ISODate );
	entry[QStringLiteral("result")] = text;
	status[run.rule.id] = entry;
	Schedules::writeStatus( status );

	ActivityLog::append( QStringLiteral("schedule.run"), run.rule.name, {
		{ QStringLiteral("action"), Schedules::actionName( run.rule.action ) },
		{ QStringLiteral("result"), text },
	} );

	const auto state = GatewayState::load();
	if( state.isTelegramConfigured() )
	{
		m_notifier->send( state.telegramToken, state.telegramChatId,
						  QStringLiteral("⏰ %1: %2\n%3\n— %4").arg( run.rule.name.isEmpty() ? Schedules::actionName( run.rule.action ) : run.rule.name,
																   Schedules::actionName( run.rule.action ), text, state.siteName ) );
	}
}



bool Scheduler::sendAction( const Schedules::Rule& rule, const ComputerControlInterface::Pointer& control )
{
	using Operation = FeatureProviderInterface::Operation;
	using Action = Schedules::Action;

	Feature::Uid uid;
	auto operation = Operation::Start;
	QVariantMap arguments;

	switch( rule.action )
	{
	case Action::PowerOn:
		return false;
	case Action::PowerDown:
		uid = SchedulePowerDownDelayedUid;
		arguments[QStringLiteral("shutdownTimeout")] = PowerDownWarningSeconds;
		break;
	case Action::Reboot:
		uid = ScheduleRebootUid;
		break;
	case Action::LockScreen:
	case Action::UnlockScreen:
		uid = ScheduleScreenLockUid;
		operation = rule.action == Action::LockScreen ? Operation::Start : Operation::Stop;
		break;
	case Action::BlockInternet:
	case Action::AllowInternet:
		uid = ScheduleInternetUid;
		operation = rule.action == Action::BlockInternet ? Operation::Start : Operation::Stop;
		break;
	case Action::BlockSites:
	case Action::UnblockSites:
		uid = ScheduleSiteFilterUid;
		operation = rule.action == Action::BlockSites ? Operation::Start : Operation::Stop;
		arguments[QStringLiteral("sites")] = rule.sites;
		break;
	case Action::Message:
		uid = ScheduleTextMessageUid;
		arguments[QStringLiteral("text")] = rule.text;
		arguments[QStringLiteral("title")] = rule.name;
		arguments[QStringLiteral("icon")] = 1;	// QMessageBox::Information
		break;
	case Action::BlockUsb:
	case Action::AllowUsb:
		uid = ScheduleDeviceControlUid;
		arguments[QStringLiteral("usb")] = rule.action == Action::BlockUsb ? QStringLiteral("block") : QStringLiteral("allow");
		break;
	case Action::BlockPrinting:
	case Action::AllowPrinting:
		uid = ScheduleDeviceControlUid;
		arguments[QStringLiteral("printer")] = rule.action == Action::BlockPrinting ? QStringLiteral("block") : QStringLiteral("allow");
		break;
	case Action::PushRoles:
	{
		uid = ScheduleAdminRolesUid;
		QFile policy( AdminRoles::path() );
		if( policy.open( QFile::ReadOnly ) == false )
		{
			return false;
		}
		arguments[QStringLiteral("policy")] = policy.readAll();
		break;
	}
	case Action::StartExam:
	case Action::EndExam:
		uid = ScheduleExamModeUid;
		operation = rule.action == Action::StartExam ? Operation::Start : Operation::Stop;
		arguments[QStringLiteral("sites")] = rule.sites;
		arguments[QStringLiteral("url")] = rule.text;
		break;
	}

	if( VeyonCore::featureManager().pluginUid( uid ).isNull() )
	{
		vWarning() << "schedule: function not available" << uid;
		return false;
	}

	VeyonCore::featureManager().controlFeature( uid, operation, arguments, { control } );
	return true;
}



QList<Scheduler::Target> Scheduler::targets( const QString& room )
{
	// testing without touching the real computers: "host1,host2"
	const auto testTargets = qEnvironmentVariable( "ARUNI_SCHEDULE_TARGETS" );
	if( testTargets.isEmpty() == false )
	{
		QList<Target> result;
		const auto hosts = testTargets.split( QLatin1Char(',') );
		for( const auto& host : hosts )
		{
			result.append( { host, host, QStringLiteral("02:00:00:00:00:01") } );
		}
		return result;
	}

	// the Configurator may have changed the computers since the server started
	VeyonCore::config().reloadFromStore();

	if( m_directory == nullptr )
	{
		m_directory = VeyonCore::networkObjectDirectoryManager().createDirectory( BuiltinDirectoryPluginUid, this );
	}
	if( m_directory == nullptr )
	{
		return {};
	}
	m_directory->update();

	QList<Target> result;
	QSet<QString> seen;
	const auto objects = m_directory->queryObjects( NetworkObject::Type::Host, NetworkObject::Attribute::None, {} );
	for( const auto& object : objects )
	{
		const auto host = object.hostAddress().isEmpty() ? object.name() : object.hostAddress();
		if( host.isEmpty() || seen.contains( host.toLower() ) || isThisComputer( host ) )
		{
			continue;
		}

		if( room.isEmpty() == false )
		{
			bool inRoom = false;
			const auto parents = m_directory->queryParents( object );
			for( const auto& parent : parents )
			{
				if( parent.type() == NetworkObject::Type::Location && parent.name() == room )
				{
					inRoom = true;
					break;
				}
			}
			if( inRoom == false )
			{
				continue;
			}
		}

		seen.insert( host.toLower() );
		result.append( { object.name().isEmpty() ? host : object.name(), host, object.macAddress() } );
	}

	return result;
}



bool Scheduler::loadCredentials()
{
	const auto keyName = MonitoringCollector::availableKeyName( GatewayState::load().sharedKeyName );
	if( keyName.isEmpty() )
	{
		return false;
	}

	if( keyName != m_loadedKey )
	{
		auto& credentials = VeyonCore::authenticationCredentials();
		if( credentials.loadPrivateKey( VeyonCore::filesystem().privateKeyPath( keyName ) ) == false )
		{
			vWarning() << "schedule: cannot load private key" << keyName;
			return false;
		}
		credentials.setAuthenticationKeyName( keyName );
		credentials.setAnnouncedUsername( QString::fromLatin1( ScheduleAnnouncedName ) );
		m_loadedKey = keyName;
	}

	return true;
}



void Scheduler::wakeOnLan( const QString& macAddress )
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

	// every network of this computer, not only the one of the default route
	QUdpSocket socket;
	socket.writeDatagram( packet, QHostAddress::Broadcast, 9 );
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& networkInterface : interfaces )
	{
		if( ( networkInterface.flags() & QNetworkInterface::CanBroadcast ) == 0 ||
			( networkInterface.flags() & QNetworkInterface::IsUp ) == 0 )
		{
			continue;
		}
		const auto entries = networkInterface.addressEntries();
		for( const auto& entry : entries )
		{
			if( entry.broadcast().isNull() == false )
			{
				socket.writeDatagram( packet, entry.broadcast(), 9 );
			}
		}
	}
}
