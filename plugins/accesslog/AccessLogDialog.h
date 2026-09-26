/*
 * AccessLogDialog.h - access log of the selected computers
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

#include <QDialog>
#include <QJsonArray>

#include "ComputerControlInterface.h"

class QTableWidget;
class AccessLogFeaturePlugin;

class AccessLogDialog : public QDialog
{
	Q_OBJECT
public:
	AccessLogDialog( AccessLogFeaturePlugin* plugin, const ComputerControlInterfaceList& computers, QWidget* parent );

	static QString eventText( const QJsonObject& entry );

private:
	void addEntries( const ComputerControlInterface::Pointer& computer, const QJsonArray& entries );
	void rebuildTable();
	bool isShown( const QJsonObject& entry ) const;
	void exportCsv();

	QTableWidget* m_table;
	class QCheckBox* m_hideGateway;
	bool m_multiple;
	QList<QPair<QString, QJsonObject>> m_entries;

};
