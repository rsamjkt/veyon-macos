/*
 * FeatureSession.h - base for interactive per-computer features of the mobile UI
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

#include <QObject>
#include <QPointer>

#include "ComputerControlInterface.h"
#include "FeatureMessage.h"

class ComputerGridModel;

// Base for the interactive per-computer features (chat, voice, application
// monitoring): the desktop plugins drive these through QWidget windows, the
// mobile UI talks to the computers directly with the plugins' feature
// messages and gets the replies through FeatureManager::featureMessageReceived()
class FeatureSession : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool available READ isAvailable CONSTANT)
	Q_PROPERTY(QString computerUid READ computerUid NOTIFY computerChanged)
	Q_PROPERTY(QString computerName READ computerName NOTIFY computerChanged)
	Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)
public:
	FeatureSession( const QString& featureName, ComputerGridModel* computers, QObject* parent = nullptr );

	// the feature's plugin is loaded (and not disabled)
	bool isAvailable() const
	{
		return m_featureUid.isNull() == false;
	}

	const QString& computerUid() const
	{
		return m_computerUid;
	}

	QString computerName() const;
	bool isOnline() const;

Q_SIGNALS:
	void computerChanged();
	void onlineChanged();

protected:
	const Feature::Uid& featureUid() const
	{
		return m_featureUid;
	}

	ComputerGridModel* computers() const
	{
		return m_computers;
	}

	// the computer the session is currently opened for (empty = closed)
	void setComputer( const QString& uid );
	ComputerControlInterface::Pointer controlInterface() const;

	// sends to the current computer; false if it is not connected
	bool sendToComputer( const FeatureMessage& message );
	static bool send( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message );

	static QString uidOf( const ComputerControlInterface::Pointer& controlInterface );

	virtual void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) = 0;

private:
	ComputerGridModel* m_computers;
	Feature::Uid m_featureUid;
	QString m_computerUid;
	QPointer<ComputerControlInterface> m_watchedInterface;
	QMetaObject::Connection m_stateConnection;

};
