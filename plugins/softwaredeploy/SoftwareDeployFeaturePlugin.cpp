/*
 * SoftwareDeployFeaturePlugin.cpp - install and remove software on many computers
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

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

#include "Filesystem.h"
#include "InstalledSoftware.h"
#include "SoftwareDeployDialog.h"
#include "SoftwareDeployFeaturePlugin.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


namespace {

constexpr int InstallTimeout = 30 * 60 * 1000;
constexpr int StaleJobTimeout = 10 * 60 * 1000;
constexpr qint64 MaxFileSize = qint64( 4 ) * 1024 * 1024 * 1024;

QByteArray fileSha256( const QString& fileName )
{
	QFile file( fileName );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	QCryptographicHash hash( QCryptographicHash::Sha256 );
	hash.addData( &file );
	return hash.result();
}



bool isSafeFileName( const QString& name )
{
	return name.isEmpty() == false && name.contains( QLatin1Char('/') ) == false && name.contains( QLatin1Char('\\') ) == false &&
		   name != QStringLiteral(".") && name != QStringLiteral("..") && name.size() < 200;
}

}



SoftwareDeployFeaturePlugin::SoftwareDeployFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_feature( Feature( QStringLiteral( "SoftwareDeploy" ),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid( FeatureUid ),
						Feature::Uid(),
						tr( "Install software" ), {},
						tr( "Install a program on the selected computers without questions, or remove an installed program." ),
						QStringLiteral(":/softwaredeploy/softwaredeploy.png") ) ),
	m_features( { m_feature } )
{
}



SoftwareDeployFeaturePlugin::~SoftwareDeployFeaturePlugin()
{
	const auto ids = m_jobs.keys();
	for( const auto& id : ids )
	{
		removeJob( id );
	}
}



const FeatureList& SoftwareDeployFeaturePlugin::featureList() const
{
	return m_features;
}



QString SoftwareDeployFeaturePlugin::defaultArguments( const QString& fileName )
{
	const auto suffix = QFileInfo( fileName ).suffix().toLower();
	if( suffix == QStringLiteral("msi") )
	{
		return QStringLiteral("/qn /norestart");
	}
	if( suffix == QStringLiteral("exe") )
	{
		return QStringLiteral("/S");
	}
	return {};
}



bool SoftwareDeployFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
												  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_feature.uid() || operation != Operation::Start )
	{
		return false;
	}

	const auto file = arguments.value( QStringLiteral("file") ).toString();
	const auto program = arguments.value( QStringLiteral("uninstall") ).toString();
	const auto extra = arguments.value( QStringLiteral("arguments") ).toString();
	if( file.isEmpty() == false )
	{
		install( file, arguments.contains( QStringLiteral("arguments") ) ? extra : defaultArguments( file ), computerControlInterfaces );
		return true;
	}
	if( program.isEmpty() == false )
	{
		uninstall( program, extra, computerControlInterfaces );
		return true;
	}
	querySoftware( computerControlInterfaces );
	return true;
}



bool SoftwareDeployFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
												const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_feature.uid() || computerControlInterfaces.isEmpty() )
	{
		return false;
	}

	SoftwareDeployDialog dialog( this, computerControlInterfaces, master.mainWindow() );
	dialog.exec();
	cancelTransfers();
	return true;
}



QStringList SoftwareDeployFeaturePlugin::commands() const
{
	return { QStringLiteral("install"), QStringLiteral("list") };
}



QString SoftwareDeployFeaturePlugin::commandHelp( const QString& command ) const
{
	if( command == QStringLiteral("install") )
	{
		return tr( "Install a program: install <host[,host...]> <file> [parameters]" );
	}
	if( command == QStringLiteral("list") )
	{
		return tr( "List the programs of a computer: list <host>" );
	}
	return {};
}



ComputerControlInterfaceList SoftwareDeployFeaturePlugin::connectComputers( const QStringList& hosts )
{
	ComputerControlInterfaceList computers;
	if( VeyonCore::instance()->initAuthentication() == false )
	{
		error( tr( "Failed to initialize credentials" ) );
		return computers;
	}

	for( const auto& host : hosts )
	{
		auto computer = ComputerControlInterface::Pointer::create( Computer( QUuid::createUuid(), host, host ) );
		computer->start( {}, ComputerControlInterface::UpdateMode::FeatureControlOnly );
		computers.append( computer );
	}

	// wait until all are connected (or failed)
	QElapsedTimer timer;
	timer.start();
	while( timer.elapsed() < 20000 )
	{
		QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
		const bool pending = std::any_of( computers.cbegin(), computers.cend(), []( const ComputerControlInterface::Pointer& c ) {
			return c->state() != ComputerControlInterface::State::Connected &&
				   c->state() != ComputerControlInterface::State::AuthenticationFailed &&
				   c->state() != ComputerControlInterface::State::AccessControlFailed;
		} );
		if( pending == false )
		{
			break;
		}
	}
	return computers;
}



CommandLinePluginInterface::RunResult SoftwareDeployFeaturePlugin::handle_install( const QStringList& arguments )
{
	if( arguments.size() < 2 )
	{
		return NotEnoughArguments;
	}
	const auto fileName = arguments.at( 1 );
	if( QFileInfo( fileName ).isReadable() == false )
	{
		error( tr( "Cannot read %1" ).arg( fileName ) );
		return Failed;
	}
	const auto parameters = arguments.size() > 2 ? arguments.mid( 2 ).join( QLatin1Char(' ') ) : defaultArguments( fileName );

	auto computers = connectComputers( arguments.at( 0 ).split( QLatin1Char(','), Qt::SkipEmptyParts ) );
	ComputerControlInterfaceList connected;
	for( const auto& computer : std::as_const( computers ) )
	{
		if( computer->state() == ComputerControlInterface::State::Connected )
		{
			connected.append( computer );
		}
		else
		{
			error( tr( "%1: not connected" ).arg( computer->computer().hostName() ) );
		}
	}

	QHash<ComputerControlInterface*, QString> states;
	connect( this, &SoftwareDeployFeaturePlugin::statusReceived, this,
			 [&states, this]( ComputerControlInterface::Pointer computer, const QString& state, qint64, int, const QString& message ) {
		if( state == QStringLiteral("done") || state == QStringLiteral("failed") )
		{
			states[computer.data()] = state;
			const auto text = QStringLiteral("%1: %2 %3").arg( computer->computer().hostName(), state, message ).trimmed();
			state == QStringLiteral("done") ? info( text ) : error( text );
		}
	} );

	install( fileName, parameters, connected );

	QElapsedTimer timer;
	timer.start();
	while( states.size() < connected.size() && timer.elapsed() < 35 * 60 * 1000 )
	{
		QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
	}

	for( const auto& computer : std::as_const( computers ) )
	{
		computer->stop();
	}

	const bool allDone = connected.size() == computers.size() &&
		std::all_of( states.cbegin(), states.cend(), []( const QString& state ) { return state == QStringLiteral("done"); } );
	return allDone && states.size() == connected.size() ? Successful : Failed;
}



CommandLinePluginInterface::RunResult SoftwareDeployFeaturePlugin::handle_list( const QStringList& arguments )
{
	if( arguments.isEmpty() )
	{
		return NotEnoughArguments;
	}

	auto computers = connectComputers( { arguments.first() } );
	if( computers.isEmpty() || computers.first()->state() != ComputerControlInterface::State::Connected )
	{
		error( tr( "%1: not connected" ).arg( arguments.first() ) );
		return Failed;
	}

	bool received = false;
	connect( this, &SoftwareDeployFeaturePlugin::softwareReceived, this,
			 [&received, this]( ComputerControlInterface::Pointer, const QJsonArray& software ) {
		for( const auto& value : software )
		{
			const auto program = value.toObject();
			print( QStringLiteral("%1\t%2\t%3").arg( program[QStringLiteral("name")].toString(),
													   program[QStringLiteral("version")].toString(),
													   program[QStringLiteral("id")].toString() ) );
		}
		received = true;
	} );
	querySoftware( computers );

	QElapsedTimer timer;
	timer.start();
	while( received == false && timer.elapsed() < 20000 )
	{
		QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
	}
	computers.first()->stop();
	return received ? NoResult : Failed;
}



// ------------------------------------------------------------------ master side

void SoftwareDeployFeaturePlugin::install( const QString& fileName, const QString& arguments,
										   const ComputerControlInterfaceList& computers )
{
	cancelTransfers();

	m_fileName = fileName;
	m_arguments = arguments;
	m_fileSize = QFileInfo( fileName ).size();
	m_fileHash = fileSha256( fileName );

	for( const auto& computer : computers )
	{
		Transfer transfer;
		transfer.computer = computer;
		transfer.job = QUuid::createUuid().toString( QUuid::WithoutBraces );
		computer->sendFeatureMessage( FeatureMessage{ m_feature.uid(), Begin }
										  .addArgument( Argument::Job, transfer.job )
										  .addArgument( Argument::Name, QFileInfo( fileName ).fileName() )
										  .addArgument( Argument::Size, m_fileSize )
										  .addArgument( Argument::Sha256, m_fileHash )
										  .addArgument( Argument::Arguments, arguments ) );
		m_transfers.insert( computer.data(), transfer );
		sendChunks( m_transfers[computer.data()] );
	}
}



void SoftwareDeployFeaturePlugin::sendChunks( Transfer& transfer )
{
	QFile file( m_fileName );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return;
	}

	while( transfer.sent < m_fileSize && transfer.sent - transfer.acknowledged < ChunksInFlight * ChunkSize )
	{
		file.seek( transfer.sent );
		const auto data = file.read( ChunkSize );
		if( data.isEmpty() )
		{
			break;
		}
		transfer.computer->sendFeatureMessage( FeatureMessage{ m_feature.uid(), Chunk }
												   .addArgument( Argument::Job, transfer.job )
												   .addArgument( Argument::Offset, transfer.sent )
												   .addArgument( Argument::Data, data ) );
		transfer.sent += data.size();
	}

	if( transfer.acknowledged >= m_fileSize && transfer.runSent == false )
	{
		transfer.runSent = true;
		transfer.computer->sendFeatureMessage( FeatureMessage{ m_feature.uid(), Run }.addArgument( Argument::Job, transfer.job ) );
	}
}



void SoftwareDeployFeaturePlugin::uninstall( const QString& programId, const QString& arguments,
											 const ComputerControlInterfaceList& computers )
{
	for( const auto& computer : computers )
	{
		computer->sendFeatureMessage( FeatureMessage{ m_feature.uid(), Uninstall }
										  .addArgument( Argument::Job, QUuid::createUuid().toString( QUuid::WithoutBraces ) )
										  .addArgument( Argument::Program, programId )
										  .addArgument( Argument::Arguments, arguments ) );
	}
}



void SoftwareDeployFeaturePlugin::querySoftware( const ComputerControlInterfaceList& computers )
{
	sendFeatureMessage( FeatureMessage{ m_feature.uid(), List }, computers );
}



void SoftwareDeployFeaturePlugin::cancelTransfers()
{
	m_transfers.clear();
}



bool SoftwareDeployFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
														const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	switch( static_cast<int>( message.command() ) )
	{
	case Status:
	{
		const auto state = message.argument( Argument::State ).toString();
		const auto received = message.argument( Argument::Received ).toLongLong();
		auto it = m_transfers.find( computerControlInterface.data() );
		if( it != m_transfers.end() && it->job == message.argument( Argument::Job ).toString() )
		{
			if( state == QStringLiteral("receiving") )
			{
				it->acknowledged = qMax( it->acknowledged, received );
				sendChunks( *it );
			}
			else if( state != QStringLiteral("installing") )
			{
				m_transfers.erase( it );
			}
		}
		Q_EMIT statusReceived( computerControlInterface, state, received, message.argument( Argument::ExitCode ).toInt(),
							   message.argument( Argument::Error ).toString() );
		break;
	}
	case Software:
		Q_EMIT softwareReceived( computerControlInterface,
								 QJsonDocument::fromJson( message.argument( Argument::SoftwareList ).toByteArray() ).array() );
		break;
	default:
		break;
	}
	return true;
}



// ------------------------------------------------------------------ server side

QString SoftwareDeployFeaturePlugin::jobDirectory( const QString& jobId )
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/deploy/") + jobId );
}



void SoftwareDeployFeaturePlugin::sendStatus( const Job& job )
{
	if( job.server )
	{
		job.server->sendFeatureMessageReply( job.context, FeatureMessage{ m_feature.uid(), Status }
			.addArgument( Argument::Job, job.id )
			.addArgument( Argument::State, job.state )
			.addArgument( Argument::Received, job.received )
			.addArgument( Argument::ExitCode, job.exitCode )
			.addArgument( Argument::Error, job.error ) );
	}
}



bool SoftwareDeployFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
														const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	const auto jobId = message.argument( Argument::Job ).toString();

	switch( static_cast<int>( message.command() ) )
	{
	case List:
		return server.sendFeatureMessageReply( messageContext, FeatureMessage{ m_feature.uid(), Software }
			.addArgument( Argument::SoftwareList,
						  QJsonDocument( InstalledSoftware::toJson( InstalledSoftware::list(), true ) ).toJson( QJsonDocument::Compact ) ) );

	case Begin:
	{
		if( QUuid::fromString( jobId ).isNull() || m_jobs.contains( jobId ) )
		{
			return true;
		}
		Job job;
		job.id = jobId;
		job.server = &server;
		job.context = messageContext;
		job.name = message.argument( Argument::Name ).toString();
		job.size = message.argument( Argument::Size ).toLongLong();
		job.sha256 = message.argument( Argument::Sha256 ).toByteArray();
		job.arguments = message.argument( Argument::Arguments ).toString();
		job.state = QStringLiteral("receiving");

		if( isSafeFileName( job.name ) == false || job.size <= 0 || job.size > MaxFileSize )
		{
			job.state = QStringLiteral("failed");
			job.error = tr( "Invalid file" );
			sendStatus( job );
			return true;
		}

		QDir().mkpath( jobDirectory( jobId ) );
		job.path = QDir( jobDirectory( jobId ) ).filePath( job.name );
		job.file = new QFile( job.path );
		if( job.file->open( QFile::WriteOnly | QFile::Truncate ) == false )
		{
			delete job.file;
			job.file = nullptr;
			job.state = QStringLiteral("failed");
			job.error = tr( "Cannot save the file on this computer" );
			sendStatus( job );
			return true;
		}

		// an interrupted upload must not stay around
		job.expiry = new QTimer( this );
		job.expiry->setSingleShot( true );
		connect( job.expiry, &QTimer::timeout, this, [this, jobId]() {
			const auto it = m_jobs.constFind( jobId );
			if( it != m_jobs.cend() && it->state == QStringLiteral("receiving") )
			{
				removeJob( jobId );
			}
		} );
		job.expiry->start( StaleJobTimeout );

		m_jobs.insert( jobId, job );
		return true;
	}

	case Chunk:
	{
		auto it = m_jobs.find( jobId );
		if( it == m_jobs.end() || it->file == nullptr )
		{
			return true;
		}
		const auto offset = message.argument( Argument::Offset ).toLongLong();
		const auto data = message.argument( Argument::Data ).toByteArray();
		if( offset != it->received || it->received + data.size() > it->size || it->file->write( data ) != data.size() )
		{
			const auto id = it->id;
			finishJob( id, -1, tr( "Transfer error" ) );
			return true;
		}
		it->received += data.size();
		it->expiry->start( StaleJobTimeout );
		it->context = messageContext;
		sendStatus( *it );
		return true;
	}

	case Run:
	{
		auto it = m_jobs.find( jobId );
		if( it == m_jobs.end() || it->file == nullptr )
		{
			return true;
		}
		it->file->close();
		delete it->file;
		it->file = nullptr;
		it->context = messageContext;

		if( it->received != it->size || fileSha256( it->path ) != it->sha256 )
		{
			finishJob( jobId, -1, tr( "The file arrived damaged" ) );
			return true;
		}
		runInstaller( *it );
		return true;
	}

	case Uninstall:
	{
		if( QUuid::fromString( jobId ).isNull() || m_jobs.contains( jobId ) )
		{
			return true;
		}
		Job job;
		job.id = jobId;
		job.server = &server;
		job.context = messageContext;
		job.arguments = message.argument( Argument::Arguments ).toString();
		m_jobs.insert( jobId, job );
		runUninstaller( m_jobs[jobId], message.argument( Argument::Program ).toString() );
		return true;
	}

	default:
		break;
	}

	return true;
}



void SoftwareDeployFeaturePlugin::runInstaller( Job& job )
{
	const auto suffix = QFileInfo( job.path ).suffix().toLower();
	const auto arguments = QProcess::splitCommand( job.arguments );

#if defined(Q_OS_WIN)
	const auto nativePath = QDir::toNativeSeparators( job.path );
	if( suffix == QStringLiteral("msi") )
	{
		startProcess( job, QStringLiteral("msiexec"), QStringList{ QStringLiteral("/i"), nativePath } + arguments );
	}
	else if( suffix == QStringLiteral("exe") )
	{
		startProcess( job, nativePath, arguments );
	}
	else
	{
		finishJob( job.id, -1, tr( "Only .msi and .exe files can be installed" ) );
	}
#elif defined(Q_OS_MACOS)
	if( suffix == QStringLiteral("pkg") )
	{
		// needs administrator rights - the user of this Mac is asked once
		const auto script = QStringLiteral("/usr/sbin/installer -pkg '%1' -target /").arg( job.path );
		startProcess( job, QStringLiteral("/usr/bin/osascript"),
					  { QStringLiteral("-e"), QStringLiteral("do shell script \"%1\" with administrator privileges").arg( script ) } );
	}
	else
	{
		finishJob( job.id, -1, tr( "Only .pkg files can be installed on a Mac" ) );
	}
#else
	Q_UNUSED(suffix)
	Q_UNUSED(arguments)
	finishJob( job.id, -1, tr( "Not supported on this operating system" ) );
#endif
}



void SoftwareDeployFeaturePlugin::runUninstaller( Job& job, const QString& programId )
{
	const auto programs = InstalledSoftware::list();
	const auto program = std::find_if( programs.cbegin(), programs.cend(), [&programId]( const InstalledSoftware::Entry& entry ) {
		return entry.id == programId;
	} );
	if( program == programs.cend() )
	{
		finishJob( job.id, -1, tr( "The program is not installed" ) );
		return;
	}

#if defined(Q_OS_WIN)
	QString command = program->quietUninstall;
	QStringList extra = QProcess::splitCommand( job.arguments );
	if( command.isEmpty() )
	{
		command = program->uninstall;
		if( command.contains( QStringLiteral("msiexec"), Qt::CaseInsensitive ) )
		{
			// "MsiExec.exe /I{GUID}" opens the maintenance dialog - /X removes quietly
			command.replace( QRegularExpression( QStringLiteral("/I\\s*\\{"), QRegularExpression::CaseInsensitiveOption ), QStringLiteral("/X{") );
			if( extra.isEmpty() )
			{
				extra = QStringList{ QStringLiteral("/qn"), QStringLiteral("/norestart") };
			}
		}
		else if( extra.isEmpty() )
		{
			// the usual installers: Inno Setup (unins000.exe) and NSIS
			const bool inno = command.contains( QRegularExpression( QStringLiteral("unins\\d+\\.exe"), QRegularExpression::CaseInsensitiveOption ) );
			extra = inno ? QStringList{ QStringLiteral("/VERYSILENT"), QStringLiteral("/SUPPRESSMSGBOXES"), QStringLiteral("/NORESTART") }
						 : QStringList{ QStringLiteral("/S") };
		}
	}

	// UninstallString is often an unquoted path with spaces
	auto parts = QProcess::splitCommand( command );
	if( parts.isEmpty() )
	{
		finishJob( job.id, -1, tr( "The program has no uninstaller" ) );
		return;
	}
	auto executable = parts.takeFirst();
	while( QFileInfo::exists( executable ) == false && parts.isEmpty() == false &&
		   executable.endsWith( QStringLiteral(".exe"), Qt::CaseInsensitive ) == false )
	{
		executable += QLatin1Char(' ') + parts.takeFirst();
	}
	startProcess( job, executable, parts + extra );
#elif defined(Q_OS_MACOS)
	// applications are moved to the trash, where they can be restored
	const auto trash = QDir::home().filePath( QStringLiteral(".Trash/") ) + QFileInfo( program->id ).fileName();
	QDir().remove( trash );
	if( QFile::rename( program->id, trash ) || QDir( program->id ).removeRecursively() )
	{
		finishJob( job.id, 0, {} );
	}
	else
	{
		finishJob( job.id, -1, tr( "Cannot remove %1 (no permission)" ).arg( program->id ) );
	}
#else
	finishJob( job.id, -1, tr( "Not supported on this operating system" ) );
#endif
}



void SoftwareDeployFeaturePlugin::startProcess( Job& job, const QString& program, const QStringList& arguments )
{
	vInfo() << "software deploy" << job.id << program << arguments;

	job.state = QStringLiteral("installing");
	sendStatus( job );

	auto process = new QProcess( this );
	job.process = process;
	const auto jobId = job.id;
	connect( process, &QProcess::finished, this, [this, jobId, process]( int exitCode, QProcess::ExitStatus status ) {
		const auto output = QString::fromUtf8( process->readAllStandardError() ).trimmed();
		finishJob( jobId, status == QProcess::NormalExit ? exitCode : -1, output.left( 300 ) );
	} );
	connect( process, &QProcess::errorOccurred, this, [this, jobId, process]( QProcess::ProcessError error ) {
		if( error == QProcess::FailedToStart )
		{
			finishJob( jobId, -1, tr( "Cannot start %1" ).arg( process->program() ) );
		}
	} );
	QTimer::singleShot( InstallTimeout, process, [process]() {
		if( process->state() != QProcess::NotRunning )
		{
			process->kill();
		}
	} );
	process->start( program, arguments );
}



void SoftwareDeployFeaturePlugin::finishJob( const QString& jobId, int exitCode, const QString& error )
{
	auto it = m_jobs.find( jobId );
	if( it == m_jobs.end() || it->state == QStringLiteral("done") || it->state == QStringLiteral("failed") )
	{
		return;
	}

	// 3010/1641: done, restart needed (msiexec and most installers)
	const bool ok = exitCode == 0 || exitCode == 3010 || exitCode == 1641;
	it->exitCode = exitCode;
	it->state = ok ? QStringLiteral("done") : QStringLiteral("failed");
	it->error = ok ? ( exitCode == 0 ? QString{} : tr( "Restart needed" ) )
				   : ( error.isEmpty() ? tr( "Exit code %1" ).arg( exitCode ) : error );
	vInfo() << "software deploy" << jobId << it->state << exitCode << it->error;
	sendStatus( *it );

	QTimer::singleShot( 0, this, [this, jobId]() { removeJob( jobId ); } );
}



void SoftwareDeployFeaturePlugin::removeJob( const QString& jobId )
{
	auto job = m_jobs.take( jobId );
	if( job.file )
	{
		job.file->close();
		delete job.file;
	}
	delete job.expiry;
	if( job.process )
	{
		job.process->deleteLater();
	}
	QDir( jobDirectory( jobId ) ).removeRecursively();
}
