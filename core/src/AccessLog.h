/*
 * AccessLog.h - who accessed this computer, when and how
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
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>

#include "VeyonCore.h"

// Written by veyon-server for every Master (desktop, app or gateway) that
// connects: authentication, access control, the features it uses and the
// disconnect. One JSON object per line in %GLOBALAPPDATA%/logs/access.jsonl,
// rotated by size. Read through the AccessLog feature plugin.
class VEYON_CORE_EXPORT AccessLog
{
public:
	static void append( const QString& event, const QString& host, const QString& user, const QJsonObject& details = {} );

	// oldest first, entries newer than since (all if invalid), at most limit (newest kept)
	static QJsonArray read( const QDateTime& since = {}, int limit = 1000 );

	static QString logPath();

	static constexpr qint64 MaxFileSize = 4 * 1024 * 1024;

private:
	static QMutex s_mutex;

};
