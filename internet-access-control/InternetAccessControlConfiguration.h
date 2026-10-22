/*
 * InternetAccessControlConfiguration.h - configuration values for Internet Access Control
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "VeyonConfiguration.h"
#include "Configuration/Proxy.h"

#define FOREACH_IAC_CONFIG_PROPERTY(OP) \
	OP( InternetAccessControlConfiguration, m_configuration, int, blockMode, setBlockMode, "BlockMode", "InternetAccessControl", 0, Configuration::Property::Flag::Standard ) \
	OP( InternetAccessControlConfiguration, m_configuration, QString, blockedPorts, setBlockedPorts, "BlockedPorts", "InternetAccessControl", QStringLiteral("80,443,8080,8443"), Configuration::Property::Flag::Standard ) \
	OP( InternetAccessControlConfiguration, m_configuration, QString, allowedSubnets, setAllowedSubnets, "AllowedSubnets", "InternetAccessControl", QStringLiteral("10.0.0.0/8,172.16.0.0/12,192.168.0.0/16,127.0.0.0/8,169.254.0.0/16"), Configuration::Property::Flag::Standard ) \
	OP( InternetAccessControlConfiguration, m_configuration, QString, allowedHosts, setAllowedHosts, "AllowedHosts", "InternetAccessControl", QString(), Configuration::Property::Flag::Standard ) \
	OP( InternetAccessControlConfiguration, m_configuration, bool, autoUnblockOnExit, setAutoUnblockOnExit, "AutoUnblockOnExit", "InternetAccessControl", true, Configuration::Property::Flag::Standard ) \
	OP( InternetAccessControlConfiguration, m_configuration, bool, blockDnsTraffic, setBlockDnsTraffic, "BlockDnsTraffic", "InternetAccessControl", false, Configuration::Property::Flag::Standard )

DECLARE_CONFIG_PROXY(InternetAccessControlConfiguration, FOREACH_IAC_CONFIG_PROPERTY)
