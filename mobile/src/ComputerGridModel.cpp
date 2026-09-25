/*
 * ComputerGridModel.cpp - computer list for the AruniControl Mobile UI
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

#include "ComputerGridModel.h"
#include "ComputerListModel.h"
#include "FeatureManager.h"
#include "VeyonCore.h"


ComputerGridModel::ComputerGridModel( QObject* parent ) :
	QSortFilterProxyModel( parent )
{
	setDynamicSortFilter( true );

	connect( this, &QAbstractItemModel::rowsInserted, this, &ComputerGridModel::countChanged );
	connect( this, &QAbstractItemModel::rowsRemoved, this, &ComputerGridModel::countChanged );
	connect( this, &QAbstractItemModel::modelReset, this, &ComputerGridModel::countChanged );
	connect( this, &QAbstractItemModel::layoutChanged, this, &ComputerGridModel::countChanged );
}



QHash<int, QByteArray> ComputerGridModel::roleNames() const
{
	return {
		{ NameRole, "name" },
		{ UidRole, "uid" },
		{ HostRole, "host" },
		{ StatusRole, "status" },
		{ StatusTextRole, "statusText" },
		{ UserRole, "user" },
		{ ScreenRevisionRole, "screenRevision" },
		{ SelectedRole, "selected" },
		{ LockedRole, "locked" },
		{ ActiveFeaturesRole, "activeFeatures" },
		{ LocationRole, "location" },
	};
}



QVariant ComputerGridModel::data( const QModelIndex& index, int role ) const
{
	if( role < NameRole )
	{
		return QSortFilterProxyModel::data( index, role );
	}

	const auto controlInterface = sourceInterface( mapToSource( index ).row() );
	if( controlInterface.isNull() )
	{
		return {};
	}

	const auto uid = controlInterface->computer().networkObjectUid().toString();

	switch( role )
	{
	case NameRole:
		return controlInterface->computerName();
	case UidRole:
		return uid;
	case HostRole:
		return controlInterface->computer().hostName();
	case StatusRole:
		return statusKey( controlInterface->state() );
	case StatusTextRole:
		switch( controlInterface->state() )
		{
		case ComputerControlInterface::State::Connected: return tr("Online");
		case ComputerControlInterface::State::Connecting: return tr("Menghubungkan…");
		case ComputerControlInterface::State::HostOffline: return tr("Offline");
		case ComputerControlInterface::State::HostNameResolutionFailed: return tr("Nama host tidak ditemukan");
		case ComputerControlInterface::State::ServerNotRunning: return tr("AruniControl belum berjalan");
		case ComputerControlInterface::State::AuthenticationFailed: return tr("Kunci akses ditolak");
		case ComputerControlInterface::State::AccessControlFailed: return tr("Akses ditolak");
		default: return tr("Tidak terhubung");
		}
	case UserRole:
	{
		const auto fullName = controlInterface->userFullName();
		return fullName.isEmpty() ? VeyonCore::stripDomain( controlInterface->userLoginName() ) : fullName;
	}
	case ScreenRevisionRole:
		return m_screenRevisions.value( uid );
	case SelectedRole:
		return m_selection.contains( uid );
	case LockedRole:
		return m_lockFeatureUid.isNull() == false &&
			   controlInterface->activeFeatures().contains( m_lockFeatureUid );
	case LocationRole:
		return controlInterface->computer().location();
	case ActiveFeaturesRole:
	{
		QStringList features;
		for( const auto& featureUid : controlInterface->activeFeatures() )
		{
			features.append( VeyonCore::featureManager().feature( featureUid ).name() );
		}
		return features;
	}
	default:
		break;
	}

	return {};
}



void ComputerGridModel::setSourceModel( QAbstractItemModel* model )
{
	if( sourceModel() )
	{
		disconnect( sourceModel(), nullptr, this, nullptr );
	}

	QSortFilterProxyModel::setSourceModel( model );

	if( model )
	{
		connect( model, &QAbstractItemModel::dataChanged, this, &ComputerGridModel::onSourceDataChanged );
		connect( model, &QAbstractItemModel::modelReset, this, [this]() {
			pruneSelection();
			Q_EMIT statsChanged();
		} );
		connect( model, &QAbstractItemModel::rowsInserted, this, &ComputerGridModel::statsChanged );
		connect( model, &QAbstractItemModel::rowsRemoved, this, [this]() {
			pruneSelection();
			Q_EMIT statsChanged();
		} );
	}

	Q_EMIT statsChanged();
}



void ComputerGridModel::setSearchText( const QString& text )
{
	if( text != m_searchText )
	{
		beginFilterChange();
		m_searchText = text;
		endFilterChange( Direction::Rows );
		Q_EMIT searchTextChanged();
		Q_EMIT countChanged();
	}
}



void ComputerGridModel::setFilter( Filter filter )
{
	if( filter != m_filter )
	{
		beginFilterChange();
		m_filter = filter;
		endFilterChange( Direction::Rows );
		Q_EMIT filterChanged();
		Q_EMIT countChanged();
	}
}



void ComputerGridModel::setLocation( const QString& location )
{
	if( location != m_location )
	{
		beginFilterChange();
		m_location = location;
		endFilterChange( Direction::Rows );
		Q_EMIT locationChanged();
		Q_EMIT countChanged();
	}
}



QStringList ComputerGridModel::locations() const
{
	QStringList locations;
	const auto rows = sourceModel() ? sourceModel()->rowCount() : 0;
	for( int row = 0; row < rows; ++row )
	{
		const auto controlInterface = sourceInterface( row );
		if( controlInterface && controlInterface->computer().location().isEmpty() == false &&
			locations.contains( controlInterface->computer().location() ) == false )
		{
			locations.append( controlInterface->computer().location() );
		}
	}

	locations.sort( Qt::CaseInsensitive );
	return locations;
}



int ComputerGridModel::onlineCount() const
{
	int online = 0;
	const auto rows = sourceModel() ? sourceModel()->rowCount() : 0;
	for( int row = 0; row < rows; ++row )
	{
		const auto controlInterface = sourceInterface( row );
		if( controlInterface && controlInterface->state() == ComputerControlInterface::State::Connected )
		{
			++online;
		}
	}
	return online;
}



int ComputerGridModel::totalCount() const
{
	return sourceModel() ? sourceModel()->rowCount() : 0;
}



void ComputerGridModel::toggleSelected( const QString& uid )
{
	setSelected( uid, m_selection.contains( uid ) == false );
}



void ComputerGridModel::setSelected( const QString& uid, bool selected )
{
	if( selected == m_selection.contains( uid ) )
	{
		return;
	}

	if( selected )
	{
		m_selection.insert( uid );
	}
	else
	{
		m_selection.remove( uid );
	}

	for( int row = 0; row < rowCount(); ++row )
	{
		const auto rowIndex = index( row, 0 );
		if( data( rowIndex, UidRole ).toString() == uid )
		{
			Q_EMIT dataChanged( rowIndex, rowIndex, { SelectedRole } );
			break;
		}
	}

	Q_EMIT selectionChanged();
}



void ComputerGridModel::selectAll()
{
	for( int row = 0; row < rowCount(); ++row )
	{
		m_selection.insert( data( index( row, 0 ), UidRole ).toString() );
	}

	if( rowCount() > 0 )
	{
		Q_EMIT dataChanged( index( 0, 0 ), index( rowCount() - 1, 0 ), { SelectedRole } );
	}
	Q_EMIT selectionChanged();
}



void ComputerGridModel::clearSelection()
{
	if( m_selection.isEmpty() )
	{
		return;
	}

	m_selection.clear();

	if( rowCount() > 0 )
	{
		Q_EMIT dataChanged( index( 0, 0 ), index( rowCount() - 1, 0 ), { SelectedRole } );
	}
	Q_EMIT selectionChanged();
}



QStringList ComputerGridModel::selectedUids() const
{
	return { m_selection.begin(), m_selection.end() };
}



QString ComputerGridModel::nameOf( const QString& uid ) const
{
	const auto controlInterface = this->controlInterface( uid );
	return controlInterface ? controlInterface->computerName() : QString{};
}



ComputerControlInterface::Pointer ComputerGridModel::controlInterface( const QString& uid ) const
{
	const auto rows = sourceModel() ? sourceModel()->rowCount() : 0;
	for( int row = 0; row < rows; ++row )
	{
		const auto controlInterface = sourceInterface( row );
		if( controlInterface && controlInterface->computer().networkObjectUid().toString() == uid )
		{
			return controlInterface;
		}
	}

	return {};
}



ComputerControlInterfaceList ComputerGridModel::controlInterfaces( const QStringList& uids ) const
{
	ComputerControlInterfaceList interfaces;
	for( const auto& uid : uids )
	{
		if( const auto controlInterface = this->controlInterface( uid ) )
		{
			interfaces.append( controlInterface );
		}
	}
	return interfaces;
}



ComputerControlInterfaceList ComputerGridModel::visibleControlInterfaces() const
{
	ComputerControlInterfaceList interfaces;
	interfaces.reserve( rowCount() );
	for( int row = 0; row < rowCount(); ++row )
	{
		if( const auto controlInterface = sourceInterface( mapToSource( index( row, 0 ) ).row() ) )
		{
			interfaces.append( controlInterface );
		}
	}
	return interfaces;
}



QString ComputerGridModel::statusKey( ComputerControlInterface::State state )
{
	switch( state )
	{
	case ComputerControlInterface::State::Connected: return QStringLiteral("online");
	case ComputerControlInterface::State::Connecting: return QStringLiteral("connecting");
	case ComputerControlInterface::State::AuthenticationFailed:
	case ComputerControlInterface::State::AccessControlFailed: return QStringLiteral("denied");
	case ComputerControlInterface::State::ServerNotRunning: return QStringLiteral("noservice");
	default: break;
	}

	return QStringLiteral("offline");
}



bool ComputerGridModel::filterAcceptsRow( int sourceRow, const QModelIndex& sourceParent ) const
{
	Q_UNUSED(sourceParent)

	const auto controlInterface = sourceInterface( sourceRow );
	if( controlInterface.isNull() )
	{
		return false;
	}

	if( m_location.isEmpty() == false && controlInterface->computer().location() != m_location )
	{
		return false;
	}

	if( m_filter == Online && controlInterface->state() != ComputerControlInterface::State::Connected )
	{
		return false;
	}

	if( m_filter == WithUser && controlInterface->userLoginName().isEmpty() )
	{
		return false;
	}

	if( m_searchText.isEmpty() )
	{
		return true;
	}

	for( const auto& text : { controlInterface->computerName(),
							  controlInterface->computer().hostName(),
							  controlInterface->userLoginName(),
							  controlInterface->userFullName() } )
	{
		if( text.contains( m_searchText, Qt::CaseInsensitive ) )
		{
			return true;
		}
	}

	return false;
}



void ComputerGridModel::onSourceDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles )
{
	const auto screenChanged = roles.isEmpty() || roles.contains( Qt::DecorationRole );

	if( screenChanged )
	{
		for( int row = topLeft.row(); row <= bottomRight.row(); ++row )
		{
			if( const auto controlInterface = sourceInterface( row ) )
			{
				++m_screenRevisions[controlInterface->computer().networkObjectUid().toString()];
			}
		}
	}

	// our roles are derived from the control interface, so any change of the
	// source row may affect all of them
	for( int row = topLeft.row(); row <= bottomRight.row(); ++row )
	{
		const auto proxyIndex = mapFromSource( sourceModel()->index( row, 0 ) );
		if( proxyIndex.isValid() )
		{
			Q_EMIT dataChanged( proxyIndex, proxyIndex );
		}
	}

	if( roles.isEmpty() || roles.contains( ComputerListModel::StateRole ) || roles.contains( Qt::DisplayRole ) )
	{
		// state and user may change filter results
		beginFilterChange();
		endFilterChange( Direction::Rows );
		Q_EMIT statsChanged();
	}
}



void ComputerGridModel::pruneSelection()
{
	const auto before = m_selection.size();

	QSet<QString> existing;
	const auto rows = sourceModel() ? sourceModel()->rowCount() : 0;
	for( int row = 0; row < rows; ++row )
	{
		if( const auto controlInterface = sourceInterface( row ) )
		{
			existing.insert( controlInterface->computer().networkObjectUid().toString() );
		}
	}

	m_selection.intersect( existing );

	if( m_selection.size() != before )
	{
		Q_EMIT selectionChanged();
	}
}



ComputerControlInterface::Pointer ComputerGridModel::sourceInterface( int sourceRow ) const
{
	if( sourceModel() == nullptr || sourceRow < 0 || sourceRow >= sourceModel()->rowCount() )
	{
		return {};
	}

	return sourceModel()->data( sourceModel()->index( sourceRow, 0 ),
								ComputerListModel::ControlInterfaceRole ).value<ComputerControlInterface::Pointer>();
}
