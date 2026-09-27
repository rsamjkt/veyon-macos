/*
 * SoftwareDeployDialog.h - install and remove software on many computers
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

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class SoftwareDeployFeaturePlugin;

// "Install software": choose an installer and its silent arguments, or pick an
// installed program to remove - with the state of every computer.
class SoftwareDeployDialog : public QDialog
{
	Q_OBJECT
public:
	SoftwareDeployDialog( SoftwareDeployFeaturePlugin* plugin, const ComputerControlInterfaceList& computers,
						  QWidget* parent = nullptr );

private:
	void chooseFile();
	void updatePreset();
	void startInstall();
	void startUninstall();
	void onStatus( ComputerControlInterface::Pointer computer, const QString& state, qint64 received,
				   int exitCode, const QString& error );
	void onSoftware( ComputerControlInterface::Pointer computer, const QJsonArray& software );
	void setState( const ComputerControlInterface::Pointer& computer, const QString& text, const QColor& color = {} );
	void resetStates( const QString& text );
	bool isBusy() const;

	SoftwareDeployFeaturePlugin* m_plugin;
	ComputerControlInterfaceList m_computers;
	qint64 m_fileSize{0};

	QTabWidget* m_tabs;
	QLineEdit* m_file;
	QComboBox* m_preset;
	QLineEdit* m_arguments;
	QPushButton* m_installButton;
	QLineEdit* m_programFilter;
	QTableWidget* m_programs;
	QLineEdit* m_uninstallArguments;
	QPushButton* m_uninstallButton;
	QTableWidget* m_states;
	QLabel* m_summary;

	// program id -> { name, version, computers having it }
	struct Program
	{
		QString id;
		QString name;
		QString version;
		int count{0};
	};
	QList<Program> m_programList;
	QHash<ComputerControlInterface*, QString> m_stateOf;

};
