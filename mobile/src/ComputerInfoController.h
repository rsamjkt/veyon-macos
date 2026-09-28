/*
 * ComputerInfoController.h - hardware, disks and programs of one computer 
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

#include <QJsonArray>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "FeatureSession.h"

// "Info & program": hardware, disks and network of one computer (Inventory
// plugin) and its installed programs, which can be removed (SoftwareDeploy
// plugin) - both by their fixed feature uids.
class ComputerInfoController : public FeatureSession
{
	Q_OBJECT
	Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
	Q_PROPERTY(QVariantList disks READ disks NOTIFY infoChanged)
	Q_PROPERTY(QVariantList network READ network NOTIFY infoChanged)
	Q_PROPERTY(QVariantList programs READ programs NOTIFY programsChanged)
	Q_PROPERTY(QString state READ state NOTIFY stateChanged)
public:
	explicit ComputerInfoController( ComputerGridModel* computers, QObject* parent = nullptr );

	// { model, serial, os, cpu, ram, user, uptime, version }
	const QVariantMap& info() const
	{
		return m_info;
	}
	// [{ path, free, total, percent }]
	const QVariantList& disks() const
	{
		return m_disks;
	}
	// [{ name, ip, mac }]
	const QVariantList& network() const
	{
		return m_network;
	}
	// [{ id, name, version, removable, state }]; state = "", "removing", "removed", "failed: ..."
	const QVariantList& programs() const
	{
		return m_programs;
	}
	// "loading", "ready", "offline", "noresponse"
	const QString& state() const
	{
		return m_state;
	}

	Q_INVOKABLE void open( const QString& uid );
	Q_INVOKABLE void close();
	Q_INVOKABLE void refresh();
	Q_INVOKABLE bool uninstall( const QString& programId );

Q_SIGNALS:
	void infoChanged();
	void programsChanged();
	void stateChanged();

protected:
	void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) override;

private:
	void handleSoftwareMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message );
	void setState( const QString& state );
	void setProgramState( const QString& id, const QString& state );

	QVariantMap m_info;
	QVariantList m_disks;
	QVariantList m_network;
	QVariantList m_programs;
	QString m_state;
	QHash<QString, QString> m_jobs;	// uninstall job -> program id
	QTimer m_timeoutTimer;

};
