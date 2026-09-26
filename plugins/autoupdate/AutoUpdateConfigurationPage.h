/*
 * AutoUpdateConfigurationPage.h - settings of the automatic updates
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

#include <QTimer>

#include "ConfigurationPage.h"

class QCheckBox;
class QLabel;
class QLineEdit;

class AutoUpdateConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit AutoUpdateConfigurationPage( QWidget* parent = nullptr );

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

private:
	void refreshStatus();

	QCheckBox* m_enabled;
	QLineEdit* m_url;
	QLabel* m_status;
	QTimer m_refreshTimer;

};
