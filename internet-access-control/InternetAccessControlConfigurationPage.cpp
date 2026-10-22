/*
 * InternetAccessControlConfigurationPage.cpp - configuration page implementation
 *
 * Part of Veyon Community Addons
 */

#include "InternetAccessControlConfiguration.h"
#include "InternetAccessControlConfigurationPage.h"
#include "Configuration/UiMapping.h"
#include "ui_InternetAccessControlConfigurationPage.h"

InternetAccessControlConfigurationPage::InternetAccessControlConfigurationPage(
	InternetAccessControlConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	ui( new Ui::InternetAccessControlConfigurationPage ),
	m_configuration( configuration )
{
	ui->setupUi( this );
	Configuration::UiMapping::setFlags( this, Configuration::Property::Flag::Standard );
}

InternetAccessControlConfigurationPage::~InternetAccessControlConfigurationPage()
{
	delete ui;
}

void InternetAccessControlConfigurationPage::resetWidgets()
{
	FOREACH_IAC_CONFIG_PROPERTY(INIT_WIDGET_FROM_PROPERTY);
}

void InternetAccessControlConfigurationPage::connectWidgetsToProperties()
{
	FOREACH_IAC_CONFIG_PROPERTY(CONNECT_WIDGET_TO_PROPERTY)
}

void InternetAccessControlConfigurationPage::applyConfiguration()
{
}
