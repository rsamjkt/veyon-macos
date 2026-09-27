/*
 * InventoryView.h - inventory of the computers in the Configurator
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

#include <QWidget>

#include "InventoryStore.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QTextBrowser;

// "Inventory" tab of the gateway page: hardware, disks and software of every
// computer the gateway reaches, with alerts and Excel export.
class InventoryView : public QWidget
{
	Q_OBJECT
public:
	explicit InventoryView( QWidget* parent = nullptr );

	void refresh();

private:
	void loadSettings();
	void saveSettings();
	void showDetails();
	void exportCsv( bool software );
	void collectNow();

	QList<InventoryStore::Record> m_records;
	QCheckBox* m_enabled;
	QSpinBox* m_offlineDays;
	QSpinBox* m_diskPercent;
	QLineEdit* m_filter;
	QTableWidget* m_table;
	QTextBrowser* m_details;
	QLabel* m_summary;
	bool m_loading{false};

};
