/*
 * GatewayConfigurationPage.h - Configurator page of the Aruni Gateway
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

#include <QDateTime>
#include <QTimer>

#include "ConfigurationPage.h"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QTabWidget;
class Notifier;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QWidget;

class GatewayConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit GatewayConfigurationPage( QWidget* parent = nullptr );

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

	static QImage qrCode( const QString& text, int moduleSize );

private:
	void startPairing();
	void refreshStatus();
	void refreshDevices();
	void removeSelectedDevice();
	void refreshRoaming();
	void renewEnrollmentCode();
	void removeSelectedLaptop();
	void applyRoaming();

	QWidget* createScreenshotBox();
	QWidget* createNotificationsTab();
	QWidget* createActivityTab();
	void refreshActivity();
	void exportActivity();
	void detectTelegramChat();
	void sendTestNotification();

	QCheckBox* m_enabled;
	QLineEdit* m_siteName;
	QLineEdit* m_relayUrl;
	QComboBox* m_sharedKey;
	QLabel* m_status;
	QLabel* m_gatewayId;

	QWidget* m_pairingBox;
	QLabel* m_qrCode;
	QLabel* m_pairingHint;
	QLineEdit* m_pairingCode;
	QPushButton* m_pairButton;

	QTableWidget* m_devices;
	QPushButton* m_removeButton;

	QWidget* m_pairBox;
	QWidget* m_devicesBox;

	// office gateway: roaming laptops
	QWidget* m_laptopsBox;
	QLineEdit* m_enrollmentCode;
	QTableWidget* m_laptops;
	QPushButton* m_removeLaptopButton;

	// this computer as roaming laptop
	QWidget* m_roamingBox;
	QCheckBox* m_roamingEnabled;
	QLineEdit* m_roamingCode;
	QLabel* m_roamingStatus;

	QTabWidget* m_tabs;
	int m_laptopsTabIndex{-1};
	int m_notificationsTabIndex{-1};
	int m_activityTabIndex{-1};

	// screenshots
	QComboBox* m_screenshotInterval;
	QSpinBox* m_screenshotRetention;
	QCheckBox* m_screenshotInOffice;
	QLabel* m_screenshotStatus;

	// notifications
	QLineEdit* m_telegramToken;
	QLineEdit* m_telegramChatId;
	QSpinBox* m_offlineAlertHours;
	QCheckBox* m_notifyRefused;
	QCheckBox* m_notifyNewLaptop;
	QLabel* m_notificationStatus;
	Notifier* m_notifier;

	// activity
	QComboBox* m_activityRange;
	QTableWidget* m_activity;
	QDateTime m_activityModified;

	QTimer m_refreshTimer;

};
