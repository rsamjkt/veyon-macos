/*
 * RolesView.h - roles of teachers and admins in the Configurator
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

#include "AdminRoles.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

// "Admin roles" tab of the gateway page: keys for teachers/admins with
// limited rooms and functions (see AdminRoles). Creates the key pair of a
// role, hands it out as installation code and sends the roles to every
// computer through the scheduler of this computer's server.
class RolesView : public QWidget
{
	Q_OBJECT
public:
	explicit RolesView( QWidget* parent = nullptr );

	void refresh();

private:
	void addRole();
	void editRole();
	void removeRole();
	void showSetupCode();
	void sendRoles();
	bool saveRoles( const QList<AdminRoles::Role>& roles );
	int selectedRow() const;
	static QString shortPermissionName( const QString& permission );

	QList<AdminRoles::Role> m_roles;
	QTableWidget* m_table;
	QPushButton* m_editButton;
	QPushButton* m_removeButton;
	QPushButton* m_codeButton;
	QLabel* m_sendStatus;

};



class RoleDialog : public QDialog
{
	Q_OBJECT
public:
	RoleDialog( const AdminRoles::Role& role, bool isNew, QWidget* parent = nullptr );

	AdminRoles::Role role() const;

	void accept() override;

	// host names/addresses of the computers of these rooms (builtin directory)
	static QStringList hostsOfRooms( const QStringList& rooms );

private:
	AdminRoles::Role m_role;
	bool m_isNew;
	QLineEdit* m_name;
	QLineEdit* m_key;
	QCheckBox* m_allRooms;
	QListWidget* m_rooms;
	QList<QPair<QString, QCheckBox*>> m_permissions;

};
