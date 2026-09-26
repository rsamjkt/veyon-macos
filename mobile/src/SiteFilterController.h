/*
 * SiteFilterController.h - block websites on computers (SiteFilter plugin)
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

#include <QTimer>
#include <QVariantList>

#include "FeatureSession.h"

// "Blokir situs": sends the SiteFilter plugin's messages (SetSites / Query) to
// the target computers and collects each computer's Status reply. Uses the
// plugin's fixed feature uid, so it works whether or not the plugin is loaded
// in the app.
class SiteFilterController : public FeatureSession
{
	Q_OBJECT
	Q_PROPERTY(QVariantList presets READ presets CONSTANT)
	Q_PROPERTY(QString targetLabel READ targetLabel NOTIFY targetsChanged)
	Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
	Q_PROPERTY(QStringList reportedSites READ reportedSites NOTIFY resultsChanged)
	Q_PROPERTY(bool busy READ isBusy NOTIFY resultsChanged)
	Q_PROPERTY(QString lastAction READ lastAction NOTIFY resultsChanged)
public:
	explicit SiteFilterController( ComputerGridModel* computers, QObject* parent = nullptr );

	// same lists as SiteFilterFeaturePlugin::presets(): [{ name, icon, sites }]
	static QVariantList presets();

	const QString& targetLabel() const
	{
		return m_label;
	}

	QVariantList results() const;
	QStringList reportedSites() const;
	bool isBusy() const;

	// "query", "block" or "unblock"
	const QString& lastAction() const
	{
		return m_action;
	}

	// "https://www.YouTube.com/watch" -> "youtube.com", empty if not a domain
	Q_INVOKABLE static QString normalizedDomain( const QString& text );

	// an empty uid list means "all visible computers"
	Q_INVOKABLE void open( const QStringList& uids, const QString& label );
	Q_INVOKABLE void close();
	Q_INVOKABLE int block( const QStringList& sites );
	Q_INVOKABLE int unblock();

Q_SIGNALS:
	void targetsChanged();
	void resultsChanged();

protected:
	void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) override;

private:
	struct Result
	{
		QString uid;
		QString name;
		QString state;	// pending, ok, error, unsupported, offline, noresponse
		QString error;
		QStringList sites;
		bool replied{false};
	};

	int sendToTargets( const FeatureMessage& message, const QString& action );
	Result* resultFor( const QString& uid );

	QStringList m_uids;
	QString m_label;
	QString m_action;
	QList<Result> m_results;
	QTimer m_timeoutTimer;

};
