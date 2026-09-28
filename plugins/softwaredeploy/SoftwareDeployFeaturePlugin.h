/*
 * SoftwareDeployFeaturePlugin.h - install and remove software on many computers
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

#pragma once

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#include "CommandLineIO.h"
#include "CommandLinePluginInterface.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "MessageContext.h"

// Installs a program on the selected computers without questions, and
// removes installed programs.
//
// Protocol (feature "SoftwareDeploy", uid below):
//   master -> server  Begin     { Job, Name, Size, Sha256, Arguments }
//   master -> server  Chunk     { Job, Offset, Data }        (acknowledged with Status)
//   master -> server  Run       { Job }                      (all data sent)
//   master -> server  Uninstall { Job, Program (id), Arguments }
//   master -> server  List
//   server -> master  Status    { Job, State, Received, ExitCode, Error }
//   server -> master  Software  { Software: JSON array with ids }
// States: receiving, installing, done, failed.
//
// Windows: runs as LocalSystem - .msi through msiexec (/qn /norestart by
// default), .exe with the given silent arguments (default /S). Uninstall
// runs the program's QuietUninstallString or UninstallString (msiexec /x /qn).
// macOS: .pkg through "installer" after the administrator password prompt;
// uninstall moves the application to the trash.
class SoftwareDeployFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface,
		CommandLinePluginInterface, CommandLineIO
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.SoftwareDeploy")
	Q_INTERFACES(PluginInterface FeatureProviderInterface CommandLinePluginInterface)
public:
	explicit SoftwareDeployFeaturePlugin( QObject* parent = nullptr );
	~SoftwareDeployFeaturePlugin() override;

	enum Command
	{
		Begin,
		Chunk,
		Run,
		Uninstall,
		List,
		Status,
		Software,
	};

	enum class Argument
	{
		Job,
		Name,
		Size,
		Sha256,
		Arguments,
		Offset,
		Data,
		State,
		Received,
		ExitCode,
		Error,
		Program,
		SoftwareList,
	};

	static constexpr auto FeatureUid = "7e4c2a19-3b8d-4f61-9a05-c6d2e8f14b37";
	static constexpr qint64 ChunkSize = 512 * 1024;
	static constexpr int ChunksInFlight = 4;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("a4f8c1d6-5e2b-4b97-8d30-6c1e9f7a2b58") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("SoftwareDeploy");
	}

	QString description() const override
	{
		return tr( "Install and remove software on the selected computers" );
	}

	QString vendor() const override
	{
		return QStringLiteral("Arunika");
	}

	QString copyright() const override
	{
		return QStringLiteral("Arunika");
	}

	const FeatureList& featureList() const override;

	QString commandLineModuleName() const override
	{
		return QStringLiteral("softwaredeploy");
	}

	QString commandLineModuleHelp() const override
	{
		return description();
	}

	QStringList commands() const override;
	QString commandHelp( const QString& command ) const override;

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	// master side
	void install( const QString& fileName, const QString& arguments, const ComputerControlInterfaceList& computers );
	void uninstall( const QString& programId, const QString& arguments, const ComputerControlInterfaceList& computers );
	void querySoftware( const ComputerControlInterfaceList& computers );
	void cancelTransfers();

	static QString defaultArguments( const QString& fileName );

public Q_SLOTS:
	CommandLinePluginInterface::RunResult handle_install( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_list( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_uninstall( const QStringList& arguments );

Q_SIGNALS:
	void statusReceived( ComputerControlInterface::Pointer computer, const QString& state, qint64 received,
						 int exitCode, const QString& error );
	void softwareReceived( ComputerControlInterface::Pointer computer, const QJsonArray& software );

private:
	// master side: one upload per computer
	struct Transfer
	{
		ComputerControlInterface::Pointer computer;
		QString job;
		qint64 sent{0};
		qint64 acknowledged{0};
		bool runSent{false};
	};

	void sendChunks( Transfer& transfer );
	ComputerControlInterfaceList connectComputers( const QStringList& hosts );

	// server side: one job per upload/uninstall
	struct Job
	{
		QString id;
		QString name;
		QString path;
		qint64 size{0};
		QByteArray sha256;
		QString arguments;
		QFile* file{nullptr};
		qint64 received{0};
		QPointer<QProcess> process;
		QString state;
		int exitCode{0};
		QString error;
		QTimer* expiry{nullptr};
		// statuses of the job go to the connection that started it
		VeyonServerInterface* server{nullptr};
		MessageContext context;
	};

	void sendStatus( const Job& job );
	void runInstaller( Job& job );
	void runUninstaller( Job& job, const QString& programId );
	void startProcess( Job& job, const QString& program, const QStringList& arguments );
	void finishJob( const QString& jobId, int exitCode, const QString& error );
	void removeJob( const QString& jobId );
	static QString jobDirectory( const QString& jobId );

	const Feature m_feature;
	const FeatureList m_features;

	QString m_fileName;
	QString m_arguments;
	qint64 m_fileSize{0};
	QByteArray m_fileHash;
	QHash<ComputerControlInterface*, Transfer> m_transfers;

	QHash<QString, Job> m_jobs;

};
