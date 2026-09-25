/*
 * ComputerGridModel.h - computer list for the AruniControl Mobile UI
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

#include <QSet>
#include <QSortFilterProxyModel>

#include "ComputerControlInterface.h"

// Sits on top of the Master's ComputerMonitoringModel and turns it into a
// flat, QML-friendly list: named roles, text search, status filter and a
// multi-selection that survives model resets.
class ComputerGridModel : public QSortFilterProxyModel
{
	Q_OBJECT
	Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
	Q_PROPERTY(Filter filter READ filter WRITE setFilter NOTIFY filterChanged)
	Q_PROPERTY(QString location READ location WRITE setLocation NOTIFY locationChanged)
	Q_PROPERTY(QStringList locations READ locations NOTIFY statsChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(int onlineCount READ onlineCount NOTIFY statsChanged)
	Q_PROPERTY(int totalCount READ totalCount NOTIFY statsChanged)
	Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
public:
	enum Filter {
		All,
		Online,
		WithUser
	};
	Q_ENUM(Filter)

	enum Role {
		NameRole = Qt::UserRole + 100,
		UidRole,
		HostRole,
		StatusRole,
		StatusTextRole,
		UserRole,
		ScreenRevisionRole,
		SelectedRole,
		LockedRole,
		ActiveFeaturesRole,
		LocationRole
	};

	explicit ComputerGridModel( QObject* parent = nullptr );

	QHash<int, QByteArray> roleNames() const override;
	QVariant data( const QModelIndex& index, int role ) const override;

	void setSourceModel( QAbstractItemModel* sourceModel ) override;

	const QString& searchText() const
	{
		return m_searchText;
	}
	void setSearchText( const QString& text );

	Filter filter() const
	{
		return m_filter;
	}
	void setFilter( Filter filter );

	const QString& location() const
	{
		return m_location;
	}
	void setLocation( const QString& location );
	QStringList locations() const;

	int count() const
	{
		return rowCount();
	}
	int onlineCount() const;
	int totalCount() const;
	int selectedCount() const
	{
		return int(m_selection.size());
	}

	Q_INVOKABLE void toggleSelected( const QString& uid );
	Q_INVOKABLE void setSelected( const QString& uid, bool selected );
	Q_INVOKABLE void selectAll();
	Q_INVOKABLE void clearSelection();
	Q_INVOKABLE QStringList selectedUids() const;
	Q_INVOKABLE QString nameOf( const QString& uid ) const;
	Q_INVOKABLE QString uidAt( int row ) const
	{
		return data( index( row, 0 ), UidRole ).toString();
	}

	ComputerControlInterface::Pointer controlInterface( const QString& uid ) const;
	ComputerControlInterfaceList controlInterfaces( const QStringList& uids ) const;
	ComputerControlInterfaceList visibleControlInterfaces() const;

	static QString statusKey( ComputerControlInterface::State state );

	void setLockFeatureUid( Feature::Uid uid )
	{
		m_lockFeatureUid = uid;
	}

Q_SIGNALS:
	void searchTextChanged();
	void filterChanged();
	void locationChanged();
	void countChanged();
	void statsChanged();
	void selectionChanged();

protected:
	bool filterAcceptsRow( int sourceRow, const QModelIndex& sourceParent ) const override;

private:
	void onSourceDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles );
	void pruneSelection();

	ComputerControlInterface::Pointer sourceInterface( int sourceRow ) const;

	QString m_searchText;
	Filter m_filter{All};
	QString m_location;
	QSet<QString> m_selection;
	QHash<QString, int> m_screenRevisions;
	Feature::Uid m_lockFeatureUid;

};
