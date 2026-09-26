/*
 * QtStandardTexts.h - texts of Qt's standard buttons for the translations
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

#include <QtGlobal>

// Qt ships no Indonesian translation of its own texts. Listing them here puts
// them into veyon_*.ts, and the application translator then also translates
// the standard dialog buttons (Apply, Reset, Cancel, ...). Only read by lupdate.
[[maybe_unused]] static const char* const QtStandardTexts[] = {
	QT_TRANSLATE_NOOP( "QPlatformTheme", "OK" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Save" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Save All" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Open" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "&Yes" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Yes to &All" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "&No" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "N&o to All" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Abort" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Retry" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Ignore" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Close" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Cancel" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Discard" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Help" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Apply" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Reset" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Restore Defaults" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Don't Save" ),
	QT_TRANSLATE_NOOP( "QPlatformTheme", "Close without Saving" ),
	QT_TRANSLATE_NOOP( "QFileDialog", "Look in:" ),
	QT_TRANSLATE_NOOP( "QFileDialog", "File &name:" ),
	QT_TRANSLATE_NOOP( "QFileDialog", "Files of type:" ),
	QT_TRANSLATE_NOOP( "QFileDialog", "&Open" ),
	QT_TRANSLATE_NOOP( "QFileDialog", "&Save" ),
	QT_TRANSLATE_NOOP( "QFileDialog", "&Choose" ),
	QT_TRANSLATE_NOOP( "QLineEdit", "&Undo" ),
	QT_TRANSLATE_NOOP( "QLineEdit", "&Redo" ),
	QT_TRANSLATE_NOOP( "QLineEdit", "Cu&t" ),
	QT_TRANSLATE_NOOP( "QLineEdit", "&Copy" ),
	QT_TRANSLATE_NOOP( "QLineEdit", "&Paste" ),
	QT_TRANSLATE_NOOP( "QLineEdit", "Delete" ),
	QT_TRANSLATE_NOOP( "QLineEdit", "Select All" ),
};
