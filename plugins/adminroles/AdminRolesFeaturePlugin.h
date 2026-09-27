/*
 * AdminRolesFeaturePlugin.h - distributes the admin roles to the computers
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

#include <QElapsedTimer>

#include "Feature.h"
#include "FeatureProviderInterface.h"

// Protocol (feature "AdminRoles", uid below, not shown in the Master):
//   admin -> server  SetPolicy { Policy: roles.json content }
//   server -> admin  Status    { Count: int, Error: QString }
// The server accepts it only from keys with full rights (ComputerControlServer
// checks the "admin" permission, which no role has). It installs the public
// keys of the roles and removes those of roles that were deleted.
// From code: controlFeature( uid, Start, { "policy": QByteArray } ).
class AdminRolesFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.AdminRoles")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit AdminRolesFeaturePlugin( QObject* parent = nullptr );
	~AdminRolesFeaturePlugin() override = default;

	enum Command
	{
		SetPolicy,
		Status,
	};

	enum class Argument
	{
		Policy,
		Count,
		Error,
	};

	static constexpr auto FeatureUid = "2f8b6d14-9c3e-4a57-b0e1-5d7a9c4f8e23";

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("b35e7d20-6a1f-4c89-8e42-9f0d3b7a6c15") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("AdminRoles");
	}

	QString description() const override
	{
		return tr( "Roles of teachers and admins with limited rights" );
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

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	// installs the policy on this computer (also used by the Configurator of the admin computer)
	static bool applyPolicy( const QByteArray& policy, QString& error );

Q_SIGNALS:
	void statusReceived( ComputerControlInterface::Pointer computerControlInterface, int count, const QString& error );

private:
	const Feature m_feature;
	const FeatureList m_features;
	QElapsedTimer m_lastNotice;

};
