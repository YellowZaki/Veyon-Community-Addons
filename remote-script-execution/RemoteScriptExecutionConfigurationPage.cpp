/*
 * RemoteScriptExecutionConfigurationPage.cpp - configuration page implementation
 *
 * Part of Veyon Community Addons
 */

#include "RemoteScriptExecutionConfiguration.h"
#include "RemoteScriptExecutionConfigurationPage.h"
#include "Configuration/UiMapping.h"
#include "ui_RemoteScriptExecutionConfigurationPage.h"

RemoteScriptExecutionConfigurationPage::RemoteScriptExecutionConfigurationPage(
	RemoteScriptExecutionConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	ui( new Ui::RemoteScriptExecutionConfigurationPage ),
	m_configuration( configuration )
{
	ui->setupUi( this );
	Configuration::UiMapping::setFlags( this, Configuration::Property::Flag::Standard );
}

RemoteScriptExecutionConfigurationPage::~RemoteScriptExecutionConfigurationPage()
{
	delete ui;
}

void RemoteScriptExecutionConfigurationPage::resetWidgets()
{
	FOREACH_REMOTESCRIPT_CONFIG_PROPERTY( INIT_WIDGET_FROM_PROPERTY );
}

void RemoteScriptExecutionConfigurationPage::connectWidgetsToProperties()
{
	FOREACH_REMOTESCRIPT_CONFIG_PROPERTY( CONNECT_WIDGET_TO_PROPERTY );
}

void RemoteScriptExecutionConfigurationPage::applyConfiguration()
{
}
