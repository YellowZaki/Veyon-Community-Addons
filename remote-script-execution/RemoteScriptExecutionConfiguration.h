/*
 * RemoteScriptExecutionConfiguration.h - configuration values for Remote Script Execution
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "VeyonConfiguration.h"
#include "Configuration/Proxy.h"

#define FOREACH_REMOTESCRIPT_CONFIG_PROPERTY(OP) \
	OP( RemoteScriptExecutionConfiguration, m_configuration, bool, runAsSystem, setRunAsSystem, "RunAsSystem", "RemoteScriptExecution", true, Configuration::Property::Flag::Standard ) \
	OP( RemoteScriptExecutionConfiguration, m_configuration, int, defaultTimeoutSeconds, setDefaultTimeoutSeconds, "DefaultTimeoutSeconds", "RemoteScriptExecution", 60, Configuration::Property::Flag::Standard ) \
	OP( RemoteScriptExecutionConfiguration, m_configuration, bool, autoCleanup, setAutoCleanup, "AutoCleanup", "RemoteScriptExecution", true, Configuration::Property::Flag::Standard ) \
	OP( RemoteScriptExecutionConfiguration, m_configuration, int, defaultInterpreter, setDefaultInterpreter, "DefaultInterpreter", "RemoteScriptExecution", 0, Configuration::Property::Flag::Standard ) \
	OP( RemoteScriptExecutionConfiguration, m_configuration, int, maxPackageSizeMb, setMaxPackageSizeMb, "MaxPackageSizeMb", "RemoteScriptExecution", 100, Configuration::Property::Flag::Standard )

DECLARE_CONFIG_PROXY(RemoteScriptExecutionConfiguration, FOREACH_REMOTESCRIPT_CONFIG_PROPERTY)
