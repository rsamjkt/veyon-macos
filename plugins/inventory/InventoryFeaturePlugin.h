/*
 * InventoryFeaturePlugin.h - hardware, disks and software of a computer
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

#include <QJsonObject>

#include "Feature.h"
#include "FeatureProviderInterface.h"

// Protocol (feature "Inventory", uid below, not shown in the Master):
//   master -> server  Query
//   server -> master  Info { Inventory: JSON (see collect()) }
class InventoryFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.Inventory")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit InventoryFeaturePlugin( QObject* parent = nullptr );
	~InventoryFeaturePlugin() override = default;

	enum Command
	{
		Query,
		Info,
	};

	enum class Argument
	{
		Inventory,
	};

	static constexpr auto FeatureUid = "4d9a7e3c-1b5f-4c28-8e60-a2f7b9d1c354";

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("e6b21f94-7c3a-4d58-b1e0-8f5c2a9d7e36") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("Inventory");
	}

	QString description() const override
	{
		return tr( "Hardware, disks and software of the computers" );
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

	// { host, os, kernel, arch, cpu, cores, ramMB, manufacturer, model, serial,
	//   uptimeHours, user, version, disks[{ name, path, totalGB, freeGB }],
	//   network[{ name, mac, ip }], software[{ name, version, publisher, installed }] }
	QJsonObject collect();

Q_SIGNALS:
	void inventoryReceived( ComputerControlInterface::Pointer computerControlInterface, const QJsonObject& inventory );

private:
	void querySerialNumber();

	const Feature m_feature;
	const FeatureList m_features;
	QString m_serialNumber;
	bool m_serialQueried{false};

};
