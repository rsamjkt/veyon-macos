/*
 * RemoteCommandFeaturePlugin.cpp - run commands on many computers
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
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include "AccessLog.h"
#include "RemoteCommandDialog.h"
#include "RemoteCommandFeaturePlugin.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


namespace {

constexpr int MaxTimeout = 30 * 60;

}



RemoteCommandFeaturePlugin::RemoteCommandFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_feature( Feature( QStringLiteral( "RemoteCommand" ),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid( FeatureUid ),
						Feature::Uid(),
						tr( "Run command" ), {},
						tr( "Run a command or script on the selected computers and see the output of each one." ),
						QStringLiteral(":/remotecommand/remotecommand.png") ) ),
	m_features( { m_feature } )
{
}



const FeatureList& RemoteCommandFeaturePlugin::featureList() const
{
	return m_features;
}



QStringList RemoteCommandFeaturePlugin::shells()
{
	return { QStringLiteral("powershell"), QStringLiteral("cmd"), QStringLiteral("sh") };
}



QString RemoteCommandFeaturePlugin::defaultShell()
{
#if defined(Q_OS_WIN)
	return QStringLiteral("powershell");
#else
	return QStringLiteral("sh");
#endif
}



QStringList RemoteCommandFeaturePlugin::commands() const
{
	return { QStringLiteral("run") };
}



QString RemoteCommandFeaturePlugin::commandHelp( const QString& command ) const
{
	if( command == QStringLiteral("run") )
	{
		return tr( "Run a command: run <host[,host...]> <command> [powershell|cmd|sh]" );
	}
	return {};
}



bool RemoteCommandFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
												 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_feature.uid() || operation != Operation::Start )
	{
		return false;
	}

	run( arguments.value( QStringLiteral("shell"), defaultShell() ).toString(),
		 arguments.value( QStringLiteral("script") ).toString(),
		 arguments.value( QStringLiteral("timeout"), 60 ).toInt(), computerControlInterfaces );
	return true;
}



bool RemoteCommandFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
											   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_feature.uid() || computerControlInterfaces.isEmpty() )
	{
		return false;
	}

	RemoteCommandDialog dialog( this, computerControlInterfaces, master.mainWindow() );
	dialog.exec();
	return true;
}



QString RemoteCommandFeaturePlugin::run( const QString& shell, const QString& script, int timeout,
										 const ComputerControlInterfaceList& computers )
{
	const auto job = QUuid::createUuid().toString( QUuid::WithoutBraces );
	sendFeatureMessage( FeatureMessage{ m_feature.uid(), Run }
							.addArgument( Argument::Job, job )
							.addArgument( Argument::Shell, shell )
							.addArgument( Argument::Script, script )
							.addArgument( Argument::Timeout, qBound( 5, timeout, MaxTimeout ) ),
						computers );
	return job;
}



bool RemoteCommandFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
													   const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Result )
	{
		Q_EMIT resultReceived( computerControlInterface, message.argument( Argument::Job ).toString(),
							   message.argument( Argument::ExitCode ).toInt(), message.argument( Argument::Output ).toString(),
							   message.argument( Argument::TimedOut ).toBool() );
	}
	return true;
}



bool RemoteCommandFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
													   const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Run )
	{
		execute( server, messageContext, message.argument( Argument::Job ).toString(),
				 message.argument( Argument::Shell ).toString(), message.argument( Argument::Script ).toString(),
				 qBound( 5, message.argument( Argument::Timeout ).toInt(), MaxTimeout ) );
	}
	return true;
}



void RemoteCommandFeaturePlugin::execute( VeyonServerInterface& server, const MessageContext& context, const QString& job,
										  const QString& shell, const QString& script, int timeout )
{
	const auto reply = [&server, context, job, this]( int exitCode, const QString& output, bool timedOut ) {
		server.sendFeatureMessageReply( context, FeatureMessage{ m_feature.uid(), Result }
			.addArgument( Argument::Job, job )
			.addArgument( Argument::ExitCode, exitCode )
			.addArgument( Argument::Output, output.right( MaxOutput ) )
			.addArgument( Argument::TimedOut, timedOut ) );
	};

	if( script.trimmed().isEmpty() )
	{
		reply( -1, tr( "Empty command" ), false );
		return;
	}

	AccessLog::append( QStringLiteral("remote_command"), {}, {}, {
		{ QStringLiteral("shell"), shell },
		{ QStringLiteral("command"), script.left( 500 ) },
	} );

	// scripts go into a file: multi-line, quoting and the command line length
	// are no problem that way
	auto tempDir = new QTemporaryDir( QDir::temp().filePath( QStringLiteral("aruni-command-XXXXXX") ) );
	QString program;
	QStringList arguments;
#if defined(Q_OS_WIN)
	if( shell == QStringLiteral("cmd") )
	{
		const auto file = tempDir->filePath( QStringLiteral("command.cmd") );
		QFile f( file );
		if( f.open( QFile::WriteOnly ) )
		{
			f.write( "@echo off\r\nchcp 65001 >nul\r\n" + script.toUtf8().replace( "\n", "\r\n" ) + "\r\n" );
		}
		program = QStringLiteral("cmd.exe");
		arguments = QStringList{ QStringLiteral("/d"), QStringLiteral("/c"), QDir::toNativeSeparators( file ) };
	}
	else
	{
		const auto file = tempDir->filePath( QStringLiteral("command.ps1") );
		QFile f( file );
		if( f.open( QFile::WriteOnly ) )
		{
			// UTF-8 with BOM, so Windows PowerShell 5 reads it as UTF-8
			f.write( "\xEF\xBB\xBF[Console]::OutputEncoding = [Text.Encoding]::UTF8\r\n" + script.toUtf8() );
		}
		program = QStringLiteral("powershell.exe");
		arguments = QStringList{ QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"),
								 QStringLiteral("Bypass"), QStringLiteral("-File"), QDir::toNativeSeparators( file ) };
	}
#else
	const auto file = tempDir->filePath( QStringLiteral("command.sh") );
	QFile f( file );
	if( f.open( QFile::WriteOnly ) )
	{
		f.write( script.toUtf8() + '\n' );
	}
	program = QStringLiteral("/bin/sh");
	arguments = QStringList{ file };
	Q_UNUSED(shell)
#endif

	auto process = new QProcess( this );
	process->setProcessChannelMode( QProcess::MergedChannels );
	process->setWorkingDirectory( tempDir->path() );
	auto timedOut = new bool( false );

	auto timer = new QTimer( process );
	timer->setSingleShot( true );
	connect( timer, &QTimer::timeout, process, [process, timedOut]() {
		*timedOut = true;
		process->kill();
	} );

	connect( process, &QProcess::finished, this, [=]( int exitCode, QProcess::ExitStatus status ) {
		const auto output = QString::fromUtf8( process->readAll() );
		reply( status == QProcess::NormalExit ? exitCode : -1, output, *timedOut );
		delete timedOut;
		process->deleteLater();
		delete tempDir;
	} );
	connect( process, &QProcess::errorOccurred, this, [=]( QProcess::ProcessError error ) {
		if( error == QProcess::FailedToStart )
		{
			reply( -1, tr( "Cannot start %1" ).arg( program ), false );
			delete timedOut;
			process->deleteLater();
			delete tempDir;
		}
	} );

	timer->start( timeout * 1000 );
	process->start( program, arguments );
}



CommandLinePluginInterface::RunResult RemoteCommandFeaturePlugin::handle_run( const QStringList& arguments )
{
	if( arguments.size() < 2 )
	{
		return NotEnoughArguments;
	}

	if( VeyonCore::instance()->initAuthentication() == false )
	{
		error( tr( "Failed to initialize credentials" ) );
		return Failed;
	}

	const auto hosts = arguments.at( 0 ).split( QLatin1Char(','), Qt::SkipEmptyParts );
	const auto shell = arguments.size() > 2 ? arguments.at( 2 ) : defaultShell();

	ComputerControlInterfaceList computers;
	for( const auto& host : hosts )
	{
		auto computer = ComputerControlInterface::Pointer::create( Computer( QUuid::createUuid(), host, host ) );
		computer->start( {}, ComputerControlInterface::UpdateMode::FeatureControlOnly );
		computers.append( computer );
	}

	QElapsedTimer timer;
	timer.start();
	while( timer.elapsed() < 20000 &&
		   std::any_of( computers.cbegin(), computers.cend(), []( const ComputerControlInterface::Pointer& c ) {
			   return c->state() != ComputerControlInterface::State::Connected &&
					  c->state() != ComputerControlInterface::State::AuthenticationFailed &&
					  c->state() != ComputerControlInterface::State::AccessControlFailed;
		   } ) )
	{
		QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
	}

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

	int results = 0;
	bool allOk = connected.size() == computers.size();
	connect( this, &RemoteCommandFeaturePlugin::resultReceived, this,
			 [&]( ComputerControlInterface::Pointer computer, const QString&, int exitCode, const QString& output, bool timedOut ) {
		++results;
		print( QStringLiteral("=== %1 (exit %2%3)").arg( computer->computer().hostName() ).arg( exitCode )
				   .arg( timedOut ? QStringLiteral(", timeout") : QString{} ) );
		print( output.trimmed() );
		allOk = allOk && exitCode == 0 && timedOut == false;
	} );

	run( shell, arguments.at( 1 ), 300, connected );

	timer.restart();
	while( results < connected.size() && timer.elapsed() < 310 * 1000 )
	{
		QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
	}

	for( const auto& computer : std::as_const( computers ) )
	{
		computer->stop();
	}
	return allOk && results == connected.size() ? Successful : Failed;
}
