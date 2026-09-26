/*
 * ComputerListFile.h - import/export of rooms and computers as a simple table
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

#include <QCoreApplication>
#include <QJsonArray>
#include <QStringList>

#include "VeyonCore.h"

// The list of rooms and computers as a table with the columns
//   Ruangan (room) | Nama (name) | Alamat IP (host) | MAC
// - what an admin types in Excel anyway. Reads CSV files saved by Excel or
// LibreOffice (separator ';', ',' or tab, detected), text copied from a
// spreadsheet (tab separated) and recognises the columns by their headings in
// Indonesian or English; without headings the order above is assumed.
class VEYON_CORE_EXPORT ComputerListFile
{
	Q_DECLARE_TR_FUNCTIONS(ComputerListFile)
public:
	struct Row
	{
		QString location;
		QString name;
		QString host;
		QString mac;
	};

	struct ParseResult
	{
		QList<Row> rows;
		QStringList problems;	// "Baris 4: alamat IP kosong"
	};

	struct MergeResult
	{
		int locationsAdded{0};
		int computersAdded{0};
		int computersUpdated{0};
	};

	static ParseResult parse( const QString& text, const QString& defaultLocation = {} );

	// adds the rows to a builtin directory object list (JSON array of
	// NetworkObjects); computers already listed in the same room with the same
	// name or address are updated instead of duplicated
	static MergeResult merge( QJsonArray& networkObjects, const QList<Row>& rows, bool replace );

	// UTF-8 with BOM and ';' so Excel with Indonesian settings opens it
	// correctly by double-click
	static QByteArray toCsv( const QJsonArray& networkObjects );
	static QByteArray templateCsv();

	static QString normalizedMac( const QString& mac );

private:
	static QStringList splitLine( const QString& line, QChar separator );

};
