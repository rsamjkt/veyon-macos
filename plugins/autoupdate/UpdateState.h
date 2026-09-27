/*
 * UpdateState.h - settings and status of the automatic updates
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

#include <QJsonObject>
#include <QString>

// Shared between the Configurator and the updater in veyon-server through
// files below %GLOBALAPPDATA%/update (like the Aruni Gateway state).
class UpdateState
{
public:
	static constexpr auto DefaultManifestUrl = "https://arunicontrol.arunihealth.id/unduh/versi.json";

	bool enabled{true};
	QString manifestUrl{QString::fromLatin1( DefaultManifestUrl )};

	static QString directory();
	static UpdateState load();
	bool save() const;

	// status written by the updater: state, version, available, error, checked
	static QJsonObject readStatus();
	static void writeStatus( const QJsonObject& status );

	// the Configurator asks the running updater to check right away
	static void requestCheck();
	static bool takeCheckRequest();

};
