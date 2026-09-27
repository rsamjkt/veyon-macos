/*
 * ExamModeDialog.cpp - settings of the exam mode
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

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "ExamModeDialog.h"
#include "ExamModeFeaturePlugin.h"


namespace {

QSettings settings()
{
	return QSettings( QStringLiteral("AruniControl"), QStringLiteral("ExamMode") );
}

}



ExamModeDialog::ExamModeDialog( int computerCount, QWidget* parent ) :
	QDialog( parent )
{
	setWindowTitle( tr( "Exam mode" ) );
	resize( 520, 640 );

	const auto stored = settings();

	auto layout = new QVBoxLayout( this );

	auto intro = new QLabel( tr( "Start the exam mode on %n selected computer(s). Ending it with the same button "
								 "restores everything.", nullptr, computerCount ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	m_blockInternet = new QCheckBox( tr( "Block the internet except for these exam websites" ) );
	m_blockInternet->setChecked( stored.value( QStringLiteral("blockInternet"), true ).toBool() );
	layout->addWidget( m_blockInternet );

	m_sites = new QPlainTextEdit;
	m_sites->setPlaceholderText( tr( "One website per line, e.g.\ncbt.school.sch.id\nforms.gle" ) );
	m_sites->setPlainText( stored.value( QStringLiteral("sites") ).toStringList().join( QLatin1Char('\n') ) );
	layout->addWidget( m_sites, 1 );

	auto sitesHint = new QLabel( tr( "Subdomains are not included automatically: if the exam website loads files from "
									 "other addresses (e.g. fonts.googleapis.com), add them too. The office network "
									 "always stays reachable." ) );
	sitesHint->setWordWrap( true );
	sitesHint->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( sitesHint );

	layout->addWidget( new QLabel( tr( "Open this website on the computers (optional):" ) ) );
	m_url = new QLineEdit( stored.value( QStringLiteral("url") ).toString() );
	m_url->setPlaceholderText( QStringLiteral("https://cbt.sekolah.sch.id") );
	layout->addWidget( m_url );

	m_closeApps = new QCheckBox( tr( "Close these applications and keep them closed:" ) );
	m_closeApps->setChecked( stored.value( QStringLiteral("closeApps"), true ).toBool() );
	layout->addWidget( m_closeApps );

	m_apps = new QPlainTextEdit;
	m_apps->setPlainText( stored.value( QStringLiteral("apps"), ExamModeFeaturePlugin::defaultApps() )
							  .toStringList().join( QLatin1Char('\n') ) );
	m_apps->setToolTip( tr( "Program names as in the Task Manager, one per line (\".exe\" is optional)" ) );
	layout->addWidget( m_apps, 1 );
	connect( m_closeApps, &QCheckBox::toggled, m_apps, &QWidget::setEnabled );
	m_apps->setEnabled( m_closeApps->isChecked() );

	m_lockKeys = new QCheckBox( tr( "Lock system shortcuts (Windows key, Alt+Tab, Alt+F4, Ctrl+Esc, Task Manager)" ) );
	m_lockKeys->setChecked( stored.value( QStringLiteral("lockKeys"), true ).toBool() );
	layout->addWidget( m_lockKeys );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
	buttons->button( QDialogButtonBox::Ok )->setText( tr( "Start exam" ) );
	connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
	layout->addWidget( buttons );
}



QVariantMap ExamModeDialog::arguments() const
{
	return {
		{ QStringLiteral("sites"), lines( m_sites ) },
		{ QStringLiteral("blockInternet"), m_blockInternet->isChecked() },
		{ QStringLiteral("url"), m_url->text().trimmed() },
		{ QStringLiteral("apps"), m_closeApps->isChecked() ? lines( m_apps ) : QStringList{} },
		{ QStringLiteral("lockKeys"), m_lockKeys->isChecked() },
	};
}



void ExamModeDialog::accept()
{
	if( m_blockInternet->isChecked() && lines( m_sites ).isEmpty() && m_url->text().trimmed().isEmpty() &&
		QMessageBox::question( this, windowTitle(),
							   tr( "No exam website is entered - the computers will have no internet at all. Continue?" ) ) != QMessageBox::Yes )
	{
		return;
	}

	auto stored = settings();
	stored.setValue( QStringLiteral("sites"), lines( m_sites ) );
	stored.setValue( QStringLiteral("blockInternet"), m_blockInternet->isChecked() );
	stored.setValue( QStringLiteral("url"), m_url->text().trimmed() );
	stored.setValue( QStringLiteral("closeApps"), m_closeApps->isChecked() );
	stored.setValue( QStringLiteral("apps"), lines( m_apps ) );
	stored.setValue( QStringLiteral("lockKeys"), m_lockKeys->isChecked() );

	QDialog::accept();
}



QStringList ExamModeDialog::lines( const QPlainTextEdit* edit )
{
	QStringList result;
	const auto all = edit->toPlainText().split( QLatin1Char('\n') );
	for( const auto& line : all )
	{
		if( line.trimmed().isEmpty() == false )
		{
			result.append( line.trimmed() );
		}
	}
	return result;
}
