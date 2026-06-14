/*
 * ChatFeaturePlugin.h - declaration of ChatFeaturePlugin
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
#include "MessageContext.h"

class ChatWidget;
class VeyonWorkerInterface;

class ChatFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.Chat")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit ChatFeaturePlugin( QObject* parent = nullptr );
	~ChatFeaturePlugin() override;

	enum class Argument
	{
		Text,
		Sender,
	};

	enum Command
	{
		ChatMessage,
	};

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("4d8b1f3a-6c2e-4a7b-9f10-3e5c7d2a8b41") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("Chat");
	}

	QString description() const override
	{
		return tr( "Exchange text messages with users in both directions." );
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

	bool handleFeatureMessageFromWorker( VeyonServerInterface& server,
										 const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

private:
	ChatWidget* masterChatFor( const ComputerControlInterface::Pointer& controlInterface );

	const Feature m_chatFeature;
	const FeatureList m_features;

	// master side: one chat window per computer
	QMap<ComputerControlInterface *, QPointer<ChatWidget>> m_masterChats;

	// server side: remember the master connection to relay worker replies back
	MessageContext m_masterContext;

	// worker side (runs in the user's session)
	QPointer<ChatWidget> m_workerChat;
	VeyonWorkerInterface* m_worker{nullptr};

};
