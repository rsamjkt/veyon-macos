/*
 * ScheduleView.h - the schedules in the Configurator
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
#include <QWidget>

#include "Schedules.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class QTimeEdit;

// "Schedules" tab of the gateway page: the list of rules with add, edit,
// delete, run now and ready-made examples. Changes are saved right away.
class ScheduleView : public QWidget
{
	Q_OBJECT
public:
	explicit ScheduleView( QWidget* parent = nullptr );

	void refresh();

private:
	void save();
	void addRule();
	void editRule();
	void removeRule();
	void runRule();
	void addExamples();
	int selectedRow() const;

	Schedules m_schedules;
	QTableWidget* m_table;
	QLabel* m_warning;
	QPushButton* m_editButton;
	QPushButton* m_removeButton;
	QPushButton* m_runButton;

};



class ScheduleRuleDialog : public QDialog
{
	Q_OBJECT
public:
	ScheduleRuleDialog( const Schedules::Rule& rule, QWidget* parent = nullptr );

	Schedules::Rule rule() const;

	void accept() override;

	static QStringList rooms();

private:
	void updateFields();

	Schedules::Rule m_rule;
	QLineEdit* m_name;
	QCheckBox* m_enabled;
	QTimeEdit* m_time;
	QList<QCheckBox*> m_days;
	QComboBox* m_action;
	QComboBox* m_room;
	QLabel* m_sitesLabel;
	QPlainTextEdit* m_sites;
	QLabel* m_textLabel;
	QLineEdit* m_text;

};
