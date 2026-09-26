/*
 * SiteFilterDialog.h - choose the websites to block
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

#include "ComputerControlInterface.h"

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class SiteFilterFeaturePlugin;

class SiteFilterDialog : public QDialog
{
	Q_OBJECT
public:
	SiteFilterDialog( SiteFilterFeaturePlugin* plugin, const ComputerControlInterfaceList& computers, QWidget* parent );

	QStringList sites() const;

private:
	void setSites( const QStringList& sites );
	void updatePresetStates();

	QPlainTextEdit* m_sitesEdit;
	QLabel* m_statusLabel;
	QList<QPair<QCheckBox*, QStringList>> m_presets;
	bool m_edited{false};
	bool m_clearAll{false};
	QStringList m_errors;

};
