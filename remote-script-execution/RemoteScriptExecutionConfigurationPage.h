/*
 * RemoteScriptExecutionConfigurationPage.h - configuration page header
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "ConfigurationPage.h"

namespace Ui {
class RemoteScriptExecutionConfigurationPage;
}

class RemoteScriptExecutionConfiguration;

class RemoteScriptExecutionConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit RemoteScriptExecutionConfigurationPage( RemoteScriptExecutionConfiguration& configuration,
													QWidget* parent = nullptr );
	~RemoteScriptExecutionConfigurationPage() override;

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

private:
	Ui::RemoteScriptExecutionConfigurationPage* ui;
	RemoteScriptExecutionConfiguration& m_configuration;
};
