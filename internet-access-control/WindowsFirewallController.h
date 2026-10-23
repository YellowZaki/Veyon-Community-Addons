/*
 * WindowsFirewallController.h - Windows Firewall controller for Internet Access Control
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QObject>

class WindowsFirewallController
{
public:
	enum class BlockMode
	{
		WebPortsOnly = 0,
		AllNonLanTraffic = 1
	};

	struct Config
	{
		BlockMode mode = BlockMode::WebPortsOnly;
		QString blockedPorts = QStringLiteral("80,443,8080,8443");
		QString allowedSubnets = QStringLiteral("10.0.0.0/8,172.16.0.0/12,192.168.0.0/16,127.0.0.0/8,169.254.0.0/16");
		QString allowedHosts;
		bool blockDns = false;
	};

	static WindowsFirewallController& instance();

	bool blockInternet( const Config& config );
	bool unblockInternet();
	bool isInternetBlocked() const;
	void cleanUp();

private:
	WindowsFirewallController() = default;
	~WindowsFirewallController() = default;

	WindowsFirewallController( const WindowsFirewallController& ) = delete;
	WindowsFirewallController& operator=( const WindowsFirewallController& ) = delete;

	bool blockViaCom( const Config& config );
	bool unblockViaCom();
	bool isBlockedViaCom() const;

	bool blockViaNetsh( const Config& config );
	bool unblockViaNetsh();

	bool m_isBlocked = false;
};
