/*
 * SetupCodeView.h - installation code in the Configurator
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

class QCheckBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

// "Installation" tab of the gateway page: creates the installation code on
// the admin computer and applies a code on another computer.
class SetupCodeView : public QWidget
{
	Q_OBJECT
public:
	explicit SetupCodeView( QWidget* parent = nullptr );

	void refresh();

private:
	void createCode();
	void saveFile();
	void applyCode();
	void updateCommand();

public:
	// the one-line agent installation like a Wazuh agent: "powershell", "cmd", "mac"
	static QString installCommand( const QString& kind, const QString& code );

	QListWidget* m_keys;
	QCheckBox* m_privateKeys;
	QCheckBox* m_computers;
	QCheckBox* m_roaming;
	QPlainTextEdit* m_code;
	QPushButton* m_copyButton;
	QPushButton* m_saveButton;
	QLabel* m_codeHint;
	class QComboBox* m_commandKind;
	QPlainTextEdit* m_command;
	QPushButton* m_copyCommandButton;
	QPlainTextEdit* m_input;

};
