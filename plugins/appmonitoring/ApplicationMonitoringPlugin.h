/*
 * ApplicationMonitoringPlugin.h - declaration of ApplicationMonitoringPlugin
 *
 * Copyright (c) 2026 Arunika / AruniControl
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

#include <QMap>
#include <QPointer>

#include "Feature.h"
#include "FeatureProviderInterface.h"

class ApplicationListDialog;

class ApplicationMonitoringPlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.ApplicationMonitoring")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit ApplicationMonitoringPlugin( QObject* parent = nullptr );
	~ApplicationMonitoringPlugin() override;

	enum class Argument
	{
		Applications,
		Frontmost,
	};

	enum Command
	{
		RequestApplications,
		ApplicationsReply,
	};

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("9a4f2c18-7b3e-4d60-a1c5-8e0f3d6b2a97") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ApplicationMonitoring");
	}

	QString description() const override
	{
		return tr( "Monitor which applications are running on selected computers." );
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

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

private:
	ApplicationListDialog* dialogFor( const ComputerControlInterface::Pointer& controlInterface );

	const Feature m_applicationMonitoringFeature;
	const FeatureList m_features;

	QMap<ComputerControlInterface *, QPointer<ApplicationListDialog>> m_dialogs;

};
