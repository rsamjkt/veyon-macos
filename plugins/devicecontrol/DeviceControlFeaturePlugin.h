/*
 * DeviceControlFeaturePlugin.h - block USB storage and printing
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

#include "Feature.h"
#include "FeatureProviderInterface.h"

// Blocks USB storage (flash drives, external disks) and printing.
//
// Protocol (feature "DeviceControl", uid below, permission "restrict"):
//   master -> server  Set    { Usb: "block"/"allow"/"", Printer: "block"/"allow"/"" }
//   master -> server  Query
//   server -> master  Status { UsbBlocked, PrinterBlocked, Supported, Error }
// Windows only: USB storage through the USBSTOR service (Start=4) and the
// "Deny all removable storage" policy - drives already plugged in stay usable
// until they are unplugged; printing by disabling and stopping the print
// spooler. Both survive a restart and are undone with "allow".
class DeviceControlFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.DeviceControl")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit DeviceControlFeaturePlugin( QObject* parent = nullptr );
	~DeviceControlFeaturePlugin() override = default;

	enum Command
	{
		Set,
		Query,
		Status,
	};

	enum class Argument
	{
		Usb,
		Printer,
		UsbBlocked,
		PrinterBlocked,
		Supported,
		Error,
	};

	static constexpr auto FeatureUid = "a6d31b7e-9f24-4c85-b0e6-3e8c5a1f7d29";

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("3b9e0c57-d84a-4f16-a2c3-7e5f1b8d6a90") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("DeviceControl");
	}

	QString description() const override
	{
		return tr( "Block USB storage and printing on the selected computers" );
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

	static bool isUsbBlocked();
	static bool isPrinterBlocked();
	static bool setUsbBlocked( bool blocked );
	static bool setPrinterBlocked( bool blocked );

Q_SIGNALS:
	void statusReceived( ComputerControlInterface::Pointer computer, bool usbBlocked, bool printerBlocked,
						 bool supported, const QString& error );

private:
	const Feature m_feature;
	const FeatureList m_features;

};
