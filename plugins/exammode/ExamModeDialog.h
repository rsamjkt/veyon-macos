/*
 * ExamModeDialog.h - settings of the exam mode
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
#include <QVariantMap>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;

// Asks for the exam websites and what else the exam mode does; remembers the
// last settings for the next exam.
class ExamModeDialog : public QDialog
{
	Q_OBJECT
public:
	ExamModeDialog( int computerCount, QWidget* parent = nullptr );

	// arguments for ExamModeFeaturePlugin::controlFeature()
	QVariantMap arguments() const;

	void accept() override;

private:
	static QStringList lines( const QPlainTextEdit* edit );

	QPlainTextEdit* m_sites;
	QCheckBox* m_blockInternet;
	QLineEdit* m_url;
	QCheckBox* m_kiosk;
	QCheckBox* m_closeApps;
	QPlainTextEdit* m_apps;
	QCheckBox* m_lockKeys;

};
