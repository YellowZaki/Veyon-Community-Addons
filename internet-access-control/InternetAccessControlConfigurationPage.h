/*
 * InternetAccessControlConfigurationPage.h - configuration page header
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "ConfigurationPage.h"

namespace Ui {
class InternetAccessControlConfigurationPage;
}

class InternetAccessControlConfiguration;

class InternetAccessControlConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit InternetAccessControlConfigurationPage( InternetAccessControlConfiguration& configuration,
													 QWidget* parent = nullptr );
	~InternetAccessControlConfigurationPage() override;

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

private:
	Ui::InternetAccessControlConfigurationPage* ui;
	InternetAccessControlConfiguration& m_configuration;
};
