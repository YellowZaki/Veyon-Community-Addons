/*
 * WindowsFirewallController.cpp - Windows Firewall controller implementation
 *
 * Part of Veyon Community Addons
 */

#include "WindowsFirewallController.h"

#ifdef Q_OS_WIN

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <initguid.h>
#include <netfw.h>
#include <comutil.h>
#include <objbase.h>

#ifndef NET_FW_IP_PROTOCOL_ANY
#define NET_FW_IP_PROTOCOL_ANY static_cast<NET_FW_IP_PROTOCOL>(256)
#endif

#endif

#include <QProcess>
#include <QRegularExpression>
#include <QHostInfo>

#include "Logger.h"

static const wchar_t* const RuleAllowVeyon = L"Veyon_IAC_Allow_Veyon";
static const wchar_t* const RuleAllowLan = L"Veyon_IAC_Allow_LAN";
static const wchar_t* const RuleBlockWebTcp = L"Veyon_IAC_Block_Web_TCP";
static const wchar_t* const RuleBlockWebTcpV6 = L"Veyon_IAC_Block_Web_TCP_V6";
static const wchar_t* const RuleBlockWebUdp = L"Veyon_IAC_Block_Web_UDP";
static const wchar_t* const RuleBlockWebUdpV6 = L"Veyon_IAC_Block_Web_UDP_V6";
static const wchar_t* const RuleBlockAllNonLan = L"Veyon_IAC_Block_All_NonLAN";
static const wchar_t* const RuleBlockAllNonLanV6 = L"Veyon_IAC_Block_All_NonLAN_V6";
static const wchar_t* const RuleBlockDns = L"Veyon_IAC_Block_DNS";
static const wchar_t* const RuleBlockDnsV6 = L"Veyon_IAC_Block_DNS_V6";
static const wchar_t* const RuleGroupVeyon = L"Veyon";

// IPv4 Public internet ranges excluding RFC 1918 (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16), loopback (127.0.0.0/8), APIPA (169.254.0.0/16), multicast (224.0.0.0/4)
static const wchar_t* const DefaultInternetRangesV4 =
	L"1.0.0.0-9.255.255.255,11.0.0.0-126.255.255.255,128.0.0.0-169.253.255.255,169.255.0.0-172.15.255.255,172.32.0.0-192.167.255.255,192.169.0.0-223.255.255.255";

// IPv6 Global Unicast public internet ranges (2000::/3) excluding link-local (fe80::/10), ULA (fc00::/7), loopback (::1), multicast (ff00::/8)
static const wchar_t* const DefaultInternetRangesV6 =
	L"2000::/3";

WindowsFirewallController& WindowsFirewallController::instance()
{
	static WindowsFirewallController s_instance;
	return s_instance;
}

bool WindowsFirewallController::blockInternet( const Config& config )
{
	vDebug() << "Activating Internet block with mode:" << static_cast<int>(config.mode);

	// Remove any existing block first to avoid duplicates
	cleanUp();

#ifdef Q_OS_WIN
	if ( blockViaCom( config ) )
	{
		m_isBlocked = true;
		vDebug() << "Successfully blocked internet access via Windows Firewall COM API";
		return true;
	}

	vWarning() << "Failed to block via COM API, falling back to netsh...";
	if ( blockViaNetsh( config ) )
	{
		m_isBlocked = true;
		return true;
	}
	return false;
#else
	vWarning() << "WindowsFirewallController is only supported on Windows";
	return false;
#endif
}

bool WindowsFirewallController::unblockInternet()
{
	vDebug() << "Deactivating Internet block";
	m_isBlocked = false;

#ifdef Q_OS_WIN
	bool ok = unblockViaCom();
	if (!ok)
	{
		vWarning() << "Unblock via COM returned false, ensuring cleanup via netsh...";
		ok = unblockViaNetsh();
	}
	return ok;
#else
	return true;
#endif
}

bool WindowsFirewallController::isInternetBlocked() const
{
#ifdef Q_OS_WIN
	return m_isBlocked || isBlockedViaCom();
#else
	return m_isBlocked;
#endif
}

void WindowsFirewallController::cleanUp()
{
	unblockInternet();
}

#ifdef Q_OS_WIN

static HRESULT initializeComPolicy( INetFwPolicy2** ppPolicy )
{
	HRESULT hr = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
	if ( hr != S_OK && hr != S_FALSE && hr != RPC_E_CHANGED_MODE )
	{
		vCritical() << "CoInitializeEx failed:" << hr;
		return hr;
	}

	hr = CoCreateInstance( CLSID_NetFwPolicy2, nullptr, CLSCTX_INPROC_SERVER,
						   IID_INetFwPolicy2, reinterpret_cast<void**>( ppPolicy ) );
	if ( FAILED( hr ) )
	{
		vCritical() << "Failed to create INetFwPolicy2 instance:" << hr;
	}
	return hr;
}

static HRESULT createAndAddRule( INetFwRules* pRules, const wchar_t* name, const wchar_t* desc,
								NET_FW_ACTION action, NET_FW_IP_PROTOCOL protocol,
								const wchar_t* remotePorts, const wchar_t* remoteAddresses )
{
	INetFwRule* pRule = nullptr;
	HRESULT hr = CoCreateInstance( CLSID_NetFwRule, nullptr, CLSCTX_INPROC_SERVER,
								   IID_INetFwRule, reinterpret_cast<void**>( &pRule ) );
	if ( FAILED( hr ) )
	{
		vCritical() << "Failed to create INetFwRule instance:" << hr;
		return hr;
	}

	BSTR bstrName = SysAllocString( name );
	BSTR bstrDesc = SysAllocString( desc );
	BSTR bstrGroup = SysAllocString( RuleGroupVeyon );

	pRule->put_Name( bstrName );
	pRule->put_Description( bstrDesc );
	pRule->put_Grouping( bstrGroup );
	pRule->put_Direction( NET_FW_RULE_DIR_OUT );
	pRule->put_Action( action );
	pRule->put_Protocol( protocol );

	if ( remotePorts && wcslen( remotePorts ) > 0 )
	{
		BSTR bstrPorts = SysAllocString( remotePorts );
		pRule->put_RemotePorts( bstrPorts );
		SysFreeString( bstrPorts );
	}

	if ( remoteAddresses && wcslen( remoteAddresses ) > 0 )
	{
		BSTR bstrAddresses = SysAllocString( remoteAddresses );
		pRule->put_RemoteAddresses( bstrAddresses );
		SysFreeString( bstrAddresses );
	}

	pRule->put_Profiles( NET_FW_PROFILE2_ALL );
	pRule->put_Enabled( VARIANT_TRUE );

	hr = pRules->Add( pRule );
	if ( FAILED( hr ) )
	{
		vCritical() << "Failed to add firewall rule" << QString::fromWCharArray(name) << ":" << hr;
	}

	SysFreeString( bstrName );
	SysFreeString( bstrDesc );
	SysFreeString( bstrGroup );
	pRule->Release();

	return hr;
}

static void removeRuleByName( INetFwRules* pRules, const wchar_t* name )
{
	BSTR bstrName = SysAllocString( name );
	INetFwRule* pRule = nullptr;
	int safety = 0;
	while ( SUCCEEDED( pRules->Item( bstrName, &pRule ) ) && pRule && ++safety <= 10 )
	{
		pRule->Release();
		pRule = nullptr;
		pRules->Remove( bstrName );
	}
	SysFreeString( bstrName );
}

bool WindowsFirewallController::blockViaCom( const Config& config )
{
	INetFwPolicy2* pPolicy = nullptr;
	HRESULT hr = initializeComPolicy( &pPolicy );
	if ( FAILED( hr ) )
	{
		return false;
	}

	INetFwRules* pRules = nullptr;
	hr = pPolicy->get_Rules( &pRules );
	if ( FAILED( hr ) || !pRules )
	{
		pPolicy->Release();
		return false;
	}

	bool allRulesAdded = true;

	if ( config.mode == BlockMode::WebPortsOnly )
	{
		QString ports = config.blockedPorts;
		if ( ports.trimmed().isEmpty() )
		{
			ports = QStringLiteral( "80,443,8080,8443" );
		}
		const std::wstring portsW = ports.toStdWString();

		// Block TCP web ports to ALL destinations (Veyon uses port 11100, SMB 445, RDP 3389)
		HRESULT hrTcp = createAndAddRule( pRules, RuleBlockWebTcp,
										  L"Veyon Internet Access Control - Block Outbound Web (TCP)",
										  NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_TCP,
										  portsW.c_str(), L"*" );

		// Block UDP web ports (HTTP/3 / QUIC) to ALL destinations
		HRESULT hrUdp = createAndAddRule( pRules, RuleBlockWebUdp,
										  L"Veyon Internet Access Control - Block Outbound Web (UDP QUIC)",
										  NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_UDP,
										  L"443,80", L"*" );

		if ( FAILED( hrTcp ) || FAILED( hrUdp ) )
		{
			allRulesAdded = false;
		}
	}
	else
	{
		// Block all outbound traffic outside local subnet
		HRESULT hrAllV4 = createAndAddRule( pRules, RuleBlockAllNonLan,
											L"Veyon Internet Access Control - Block All Outbound Non-LAN (IPv4)",
											NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_ANY,
											nullptr, DefaultInternetRangesV4 );

		HRESULT hrAllV6 = createAndAddRule( pRules, RuleBlockAllNonLanV6,
											L"Veyon Internet Access Control - Block All Outbound Non-LAN (IPv6)",
											NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_ANY,
											nullptr, DefaultInternetRangesV6 );

		if ( FAILED( hrAllV4 ) || FAILED( hrAllV6 ) )
		{
			allRulesAdded = false;
		}
	}

	// Optional DNS block
	if ( config.blockDns )
	{
		HRESULT hrDns = createAndAddRule( pRules, RuleBlockDns,
										  L"Veyon Internet Access Control - Block Outbound DNS",
										  NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_UDP,
										  L"53", L"*" );
		if ( FAILED( hrDns ) )
		{
			allRulesAdded = false;
		}
	}

	pRules->Release();
	pPolicy->Release();

	return allRulesAdded;
}

bool WindowsFirewallController::unblockViaCom()
{
	INetFwPolicy2* pPolicy = nullptr;
	HRESULT hr = initializeComPolicy( &pPolicy );
	if ( FAILED( hr ) )
	{
		return false;
	}

	INetFwRules* pRules = nullptr;
	hr = pPolicy->get_Rules( &pRules );
	if ( FAILED( hr ) || !pRules )
	{
		pPolicy->Release();
		return false;
	}

	removeRuleByName( pRules, RuleAllowVeyon );
	removeRuleByName( pRules, RuleAllowLan );
	removeRuleByName( pRules, RuleBlockWebTcp );
	removeRuleByName( pRules, RuleBlockWebTcpV6 );
	removeRuleByName( pRules, RuleBlockWebUdp );
	removeRuleByName( pRules, RuleBlockWebUdpV6 );
	removeRuleByName( pRules, RuleBlockAllNonLan );
	removeRuleByName( pRules, RuleBlockAllNonLanV6 );
	removeRuleByName( pRules, RuleBlockDns );
	removeRuleByName( pRules, RuleBlockDnsV6 );

	// Remove whitelist host exceptions if any
	for ( int i = 1; i <= 16; ++i )
	{
		const auto ruleName = QString( "Veyon_IAC_Allow_Host_%1" ).arg( i ).toStdWString();
		removeRuleByName( pRules, ruleName.c_str() );
	}

	pRules->Release();
	pPolicy->Release();
	return true;
}

bool WindowsFirewallController::isBlockedViaCom() const
{
	INetFwPolicy2* pPolicy = nullptr;
	HRESULT hr = initializeComPolicy( &pPolicy );
	if ( FAILED( hr ) )
	{
		return false;
	}

	INetFwRules* pRules = nullptr;
	hr = pPolicy->get_Rules( &pRules );
	if ( FAILED( hr ) || !pRules )
	{
		pPolicy->Release();
		return false;
	}

	bool blocked = false;
	INetFwRule* pRule = nullptr;

	BSTR bstrWebTcp = SysAllocString( RuleBlockWebTcp );
	if ( SUCCEEDED( pRules->Item( bstrWebTcp, &pRule ) ) && pRule )
	{
		VARIANT_BOOL enabled = VARIANT_FALSE;
		pRule->get_Enabled( &enabled );
		blocked = ( enabled == VARIANT_TRUE );
		pRule->Release();
		pRule = nullptr;
	}
	SysFreeString( bstrWebTcp );

	if ( !blocked )
	{
		BSTR bstrAllNonLan = SysAllocString( RuleBlockAllNonLan );
		if ( SUCCEEDED( pRules->Item( bstrAllNonLan, &pRule ) ) && pRule )
		{
			VARIANT_BOOL enabled = VARIANT_FALSE;
			pRule->get_Enabled( &enabled );
			blocked = ( enabled == VARIANT_TRUE );
			pRule->Release();
			pRule = nullptr;
		}
		SysFreeString( bstrAllNonLan );
	}

	pRules->Release();
	pPolicy->Release();

	return blocked;
}

#endif // Q_OS_WIN

bool WindowsFirewallController::blockViaNetsh( const Config& config )
{
	if ( config.mode == BlockMode::WebPortsOnly )
	{
		QString ports = config.blockedPorts;
		if ( ports.trimmed().isEmpty() )
		{
			ports = QStringLiteral("80,443,8080,8443");
		}
		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_Web_TCP"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=%1").arg(ports),
			  QStringLiteral("remoteip=any"), QStringLiteral("enable=yes") } );

		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_Web_UDP"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=443,80"),
			  QStringLiteral("remoteip=any"), QStringLiteral("enable=yes") } );
	}
	else
	{
		const QString rangeV4 = QString::fromWCharArray( DefaultInternetRangesV4 );
		const QString rangeV6 = QString::fromWCharArray( DefaultInternetRangesV6 );

		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_All_NonLAN"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("protocol=ANY"), QStringLiteral("remoteip=%1").arg(rangeV4),
			  QStringLiteral("enable=yes") } );

		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_All_NonLAN_V6"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("protocol=ANY"), QStringLiteral("remoteip=%1").arg(rangeV6),
			  QStringLiteral("enable=yes") } );
	}

	if ( config.blockDns )
	{
		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_DNS"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=53"),
			  QStringLiteral("remoteip=any"), QStringLiteral("enable=yes") } );
	}

	return true;
}

bool WindowsFirewallController::unblockViaNetsh()
{
	const QStringList ruleNames = {
		QStringLiteral("Veyon_IAC_Allow_Veyon"),
		QStringLiteral("Veyon_IAC_Allow_LAN"),
		QStringLiteral("Veyon_IAC_Block_Web_TCP"),
		QStringLiteral("Veyon_IAC_Block_Web_TCP_V6"),
		QStringLiteral("Veyon_IAC_Block_Web_UDP"),
		QStringLiteral("Veyon_IAC_Block_Web_UDP_V6"),
		QStringLiteral("Veyon_IAC_Block_All_NonLAN"),
		QStringLiteral("Veyon_IAC_Block_All_NonLAN_V6"),
		QStringLiteral("Veyon_IAC_Block_DNS"),
		QStringLiteral("Veyon_IAC_Block_DNS_V6")
	};

	for ( const auto& name : ruleNames )
	{
		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
			  QStringLiteral("rule"), QStringLiteral("name=%1").arg(name) } );
	}

	return true;
}
