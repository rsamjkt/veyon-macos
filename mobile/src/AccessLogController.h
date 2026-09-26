/*
 * AccessLogController.h - access log of one computer (AccessLog plugin)
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

#include "FeatureSession.h"

// "Log akses": who connected to one computer, when and which functions were
// used. Queries the AccessLog plugin (fixed feature uid) and turns the JSON
// entries into ready-to-show Indonesian rows, newest first.
class AccessLogController : public FeatureSession
{
	Q_OBJECT
	Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
	Q_PROPERTY(QString state READ state NOTIFY stateChanged)
	Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY entriesChanged)
public:
	explicit AccessLogController( ComputerGridModel* computers, QObject* parent = nullptr );

	// [{ time, day, title, detail, icon, kind }]; kind = ok, info, warning, danger
	const QVariantList& entries() const
	{
		return m_entries;
	}

	// "loading", "ready", "offline" or "noresponse"
	const QString& state() const
	{
		return m_state;
	}

	const QString& updatedAt() const
	{
		return m_updatedAt;
	}

	Q_INVOKABLE void open( const QString& uid );
	Q_INVOKABLE void close();
	Q_INVOKABLE void refresh();

	// "1 j 5 mnt", "3 mnt 20 dtk", "12 dtk"
	static QString formatDuration( qint64 seconds );

Q_SIGNALS:
	void entriesChanged();
	void stateChanged();

protected:
	void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) override;

private:
	void setEntries( const QJsonArray& entries );
	void setState( const QString& state );
	static QString featureLabel( const QString& feature );

	static constexpr int Limit = 500;

	QVariantList m_entries;
	QString m_state;
	QString m_updatedAt;
	QTimer m_timeoutTimer;

};
