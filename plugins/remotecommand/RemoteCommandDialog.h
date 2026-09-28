/*
 * RemoteCommandDialog.h - run commands on many computers
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
#include <QHash>

#include "ComputerControlInterface.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTextBrowser;
class RemoteCommandFeaturePlugin;

class RemoteCommandDialog : public QDialog
{
	Q_OBJECT
public:
	RemoteCommandDialog( RemoteCommandFeaturePlugin* plugin, const ComputerControlInterfaceList& computers,
						 QWidget* parent = nullptr );

private:
	void runCommand();
	void onResult( ComputerControlInterface::Pointer computer, const QString& job, int exitCode,
				   const QString& output, bool timedOut );
	void showOutput();

	RemoteCommandFeaturePlugin* m_plugin;
	ComputerControlInterfaceList m_computers;
	QString m_job;
	QHash<ComputerControlInterface*, QString> m_outputs;

	QComboBox* m_examples;
	QComboBox* m_shell;
	QPlainTextEdit* m_script;
	QSpinBox* m_timeout;
	QTableWidget* m_results;
	QTextBrowser* m_output;
	QLabel* m_summary;

};
