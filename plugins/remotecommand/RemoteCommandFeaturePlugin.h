/*
 * RemoteCommandFeaturePlugin.h - run commands on many computers
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

#include <QHash>
#include <QPointer>
#include <QProcess>

#include "CommandLineIO.h"
#include "CommandLinePluginInterface.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "MessageContext.h"

// Runs a command or script on the selected computers and returns the output.
//
// Protocol (feature "RemoteCommand", uid below, permission "commands" of the
// admin roles):
//   master -> server  Run    { Job, Shell, Script, Timeout (s) }
//   server -> master  Result { Job, ExitCode, Output, TimedOut }
// Shells: "powershell" and "cmd" (Windows, run as LocalSystem), "sh"
// (macOS, run as the logged-on user). Every command is written to the access
// log of the computer with its text.
class RemoteCommandFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface,
		CommandLinePluginInterface, CommandLineIO
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.RemoteCommand")
	Q_INTERFACES(PluginInterface FeatureProviderInterface CommandLinePluginInterface)
public:
	explicit RemoteCommandFeaturePlugin( QObject* parent = nullptr );
	~RemoteCommandFeaturePlugin() override = default;

	enum Command
	{
		Run,
		Result,
	};

	enum class Argument
	{
		Job,
		Shell,
		Script,
		Timeout,
		ExitCode,
		Output,
		TimedOut,
	};

	static constexpr auto FeatureUid = "c81e5f3a-2d97-4b06-9e14-7a3f0b6d2c58";
	static constexpr int MaxOutput = 256 * 1024;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("d5a17e83-6c29-4f40-b8d1-2e9f5c3a7b64") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("RemoteCommand");
	}

	QString description() const override
	{
		return tr( "Run commands on the selected computers" );
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
		return QStringLiteral("remotecommand");
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

	// master side, returns the job id
	QString run( const QString& shell, const QString& script, int timeout, const ComputerControlInterfaceList& computers );

	static QStringList shells();
	static QString defaultShell();

public Q_SLOTS:
	CommandLinePluginInterface::RunResult handle_run( const QStringList& arguments );

Q_SIGNALS:
	void resultReceived( ComputerControlInterface::Pointer computer, const QString& job, int exitCode,
						 const QString& output, bool timedOut );

private:
	void execute( VeyonServerInterface& server, const MessageContext& context, const QString& job,
				  const QString& shell, const QString& script, int timeout );

	const Feature m_feature;
	const FeatureList m_features;

};
