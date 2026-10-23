/*
 * ScreenRecorderConfigurationPage.h - configuration page header
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "ConfigurationPage.h"

namespace Ui {
class ScreenRecorderConfigurationPage;
}

class ScreenRecorderConfiguration;

class ScreenRecorderConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit ScreenRecorderConfigurationPage( ScreenRecorderConfiguration& configuration,
											 QWidget* parent = nullptr );
	~ScreenRecorderConfigurationPage() override;

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

private Q_SLOTS:
	void browseOutputDirectory();
	void updateWidgetStates();

private:
	Ui::ScreenRecorderConfigurationPage* ui;
	ScreenRecorderConfiguration& m_configuration;
};
