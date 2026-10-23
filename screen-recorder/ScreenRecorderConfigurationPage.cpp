/*
 * ScreenRecorderConfigurationPage.cpp - configuration page implementation
 *
 * Part of Veyon Community Addons
 */

#include "ScreenRecorderConfiguration.h"
#include "ScreenRecorderConfigurationPage.h"
#include "Configuration/UiMapping.h"
#include "ui_ScreenRecorderConfigurationPage.h"

#include <QFileDialog>

ScreenRecorderConfigurationPage::ScreenRecorderConfigurationPage(
	ScreenRecorderConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	ui( new Ui::ScreenRecorderConfigurationPage ),
	m_configuration( configuration )
{
	ui->setupUi( this );
	Configuration::UiMapping::setFlags( this, Configuration::Property::Flag::Standard );

	connect( ui->browseButton, &QPushButton::clicked,
			 this, &ScreenRecorderConfigurationPage::browseOutputDirectory );
	connect( ui->useFixedResolution, &QCheckBox::toggled,
			 this, &ScreenRecorderConfigurationPage::updateWidgetStates );
	connect( ui->useFixedFramerate, &QCheckBox::toggled,
			 this, &ScreenRecorderConfigurationPage::updateWidgetStates );
}

ScreenRecorderConfigurationPage::~ScreenRecorderConfigurationPage()
{
	delete ui;
}

void ScreenRecorderConfigurationPage::resetWidgets()
{
	FOREACH_SCREENRECORDER_CONFIG_PROPERTY( INIT_WIDGET_FROM_PROPERTY );
	updateWidgetStates();
}

void ScreenRecorderConfigurationPage::connectWidgetsToProperties()
{
	FOREACH_SCREENRECORDER_CONFIG_PROPERTY( CONNECT_WIDGET_TO_PROPERTY );
}

void ScreenRecorderConfigurationPage::applyConfiguration()
{
}

void ScreenRecorderConfigurationPage::browseOutputDirectory()
{
	const QString currentDir = ui->outputDirectory->text().isEmpty()
		? ScreenRecorderUtil::resolvedOutputDirectory( m_configuration )
		: ui->outputDirectory->text();

	const QString dir = QFileDialog::getExistingDirectory(
		this,
		tr( "Select Output Directory" ),
		currentDir,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );

	if ( !dir.isEmpty() )
	{
		ui->outputDirectory->setText( dir );
	}
}

void ScreenRecorderConfigurationPage::updateWidgetStates()
{
	const bool fixedRes = ui->useFixedResolution->isChecked();
	ui->resolutionWidth->setEnabled( fixedRes );
	ui->resolutionHeight->setEnabled( fixedRes );
	ui->keepAspectRatio->setEnabled( fixedRes );

	const bool fixedFps = ui->useFixedFramerate->isChecked();
	ui->framesPerSecond->setEnabled( fixedFps );
}
