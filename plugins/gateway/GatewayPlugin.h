/*
 * GatewayPlugin.h - Aruni Gateway: reach this LAN's computers via the Aruni Relay
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

#include "CommandLineIO.h"
#include "CommandLinePluginInterface.h"
#include "ConfigurationPagePluginInterface.h"
#include "PluginInterface.h"

class GatewayService;
class RoamingAgent;
class Scheduler;

class GatewayPlugin : public QObject, PluginInterface, ConfigurationPagePluginInterface,
		CommandLinePluginInterface, CommandLineIO
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.Gateway")
	Q_INTERFACES(PluginInterface ConfigurationPagePluginInterface CommandLinePluginInterface)
public:
	explicit GatewayPlugin( QObject* parent = nullptr );
	~GatewayPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("8e1c7d32-5a4b-4f0e-9c6d-2b7a1e3f9d54") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "AruniGateway" );
	}

	QString description() const override
	{
		return tr( "Access the computers of this network from the AruniControl app over the internet" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Arunika" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "AruniControl Community" );
	}

	ConfigurationPage* createConfigurationPage() override;

	QString commandLineModuleName() const override
	{
		return QStringLiteral( "gateway" );
	}

	QString commandLineModuleHelp() const override
	{
		return tr( "Commands for the Aruni Gateway and roaming laptops" );
	}

	QStringList commands() const override;
	QString commandHelp( const QString& command ) const override;

public Q_SLOTS:
	CommandLinePluginInterface::RunResult handle_status( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_enroll( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_leave( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_enrollmentcode( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_runroaming( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_activity( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_runschedules( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_setupcode( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_setup( const QStringList& arguments );

private:
	GatewayService* m_service{nullptr};
	RoamingAgent* m_roamingAgent{nullptr};
	Scheduler* m_scheduler{nullptr};
	QMap<QString, QString> m_commands;

};
