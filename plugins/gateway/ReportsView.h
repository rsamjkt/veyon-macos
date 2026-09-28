/*
 * ReportsView.h - reports tab in the Configurator
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
#include "Reports.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QTableWidget;
class QTextBrowser;
class QTimeEdit;

// "Reports" tab of the gateway page: attendance, application usage and the
// daily report to Telegram.
class ReportsView : public QWidget
{
	Q_OBJECT
public:
	explicit ReportsView( QWidget* parent = nullptr );

	void refresh();

private:
	QWidget* createAttendanceTab();
	QWidget* createUsageTab();
	QWidget* createDailyReportTab();
	void refreshAttendance();
	void refreshUsage();
	void refreshDailyReport();
	void saveDailyReportSettings();
	static QPair<QDate, QDate> range( const QComboBox* combo );
	static QComboBox* createRangeCombo();

	QComboBox* m_attendanceRange;
	QTableWidget* m_attendance;
	QLabel* m_attendanceSummary;
	QList<Reports::Session> m_sessions;

	QComboBox* m_usageRange;
	QComboBox* m_usageComputer;
	QTableWidget* m_usageTable;
	QLabel* m_usageSummary;
	QList<Reports::Usage> m_usage;

	QCheckBox* m_reportEnabled;
	QTimeEdit* m_reportTime;
	QTextBrowser* m_reportPreview;
	bool m_loading{false};

};
