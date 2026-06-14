/*
 * AruniMediaFeaturePlugin.h - declaration of AruniMediaFeaturePlugin
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

#include "Feature.h"
#include "FeatureProviderInterface.h"

// AruniMedia - media-device control for client computers (an open replacement
// for Veyon's commercial Auvidus add-on). On macOS only audio control is
// possible without an MDM profile / kernel extension; muting webcams and USB
// devices system-wide requires privileged components that are out of scope here.
class AruniMediaFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.AruniMedia")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit AruniMediaFeaturePlugin( QObject* parent = nullptr );
	~AruniMediaFeaturePlugin() override = default;

	enum Command
	{
		MuteAudio,
		UnmuteAudio,
	};

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("5e1d4b8a-3c7f-49a2-b6d0-1f8e2c5a9d40") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("AruniMedia");
	}

	QString description() const override
	{
		return tr( "Control the audio of selected computers (mute/unmute)." );
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

	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

private:
	void setAudioMuted( bool muted );

	const Feature m_muteAudioFeature;
	const FeatureList m_features;

};
