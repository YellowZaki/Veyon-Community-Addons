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

static const wchar_t* const RuleBlockWebTcp = L"Veyon_IAC_Block_Web_TCP";
static const wchar_t* const RuleBlockWebUdp = L"Veyon_IAC_Block_Web_UDP";
static const wchar_t* const RuleBlockAllNonLan = L"Veyon_IAC_Block_All_NonLAN";
static const wchar_t* const RuleBlockDns = L"Veyon_IAC_Block_DNS";
static const wchar_t* const RuleGroupVeyon = L"Veyon";

// IPv4 Public internet ranges excluding RFC 1918 (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 127.0.0.0/8, 169.254.0.0/16)
static const wchar_t* const DefaultInternetRanges =
	L"0.0.0.0-9.255.255.255,11.0.0.0-172.15.255.255,172.32.0.0-192.167.255.255,192.169.0.0-255.255.255.255";

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
		vDebug() << "Successfully blocked internet access via Windows Firewall COM API";
		return true;
	}

	vWarning() << "Failed to block via COM API, falling back to netsh...";
	return blockViaNetsh( config );
#else
	vWarning() << "WindowsFirewallController is only supported on Windows";
	return false;
#endif
}

bool WindowsFirewallController::unblockInternet()
{
	vDebug() << "Deactivating Internet block";

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
	return isBlockedViaCom();
#else
	return false;
#endif
}

void WindowsFirewallController::cleanUp()
{
#ifdef Q_OS_WIN
	unblockViaCom();
	unblockViaNetsh();
#endif
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
	pRules->Remove( bstrName );
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

	const std::wstring portsW = config.blockedPorts.toStdWString();
	const wchar_t* remoteRange = DefaultInternetRanges;

	if ( config.mode == BlockMode::WebPortsOnly )
	{
		// Block TCP web ports to Internet
		hr = createAndAddRule( pRules, RuleBlockWebTcp,
							   L"Veyon Internet Access Control - Block Outbound Web (TCP)",
							   NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_TCP,
							   portsW.c_str(), remoteRange );

		// Block UDP web ports (HTTP/3 / QUIC) to Internet
		if ( SUCCEEDED( hr ) )
		{
			createAndAddRule( pRules, RuleBlockWebUdp,
							  L"Veyon Internet Access Control - Block Outbound Web (UDP)",
							  NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_UDP,
							  L"443,80", remoteRange );
		}
	}
	else
	{
		// Block all outbound traffic outside local subnet
		hr = createAndAddRule( pRules, RuleBlockAllNonLan,
							   L"Veyon Internet Access Control - Block All Outbound Non-LAN",
							   NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_ANY,
							   nullptr, remoteRange );
	}

	// Optional DNS block to prevent DNS leak/tunnels
	if ( config.blockDns )
	{
		createAndAddRule( pRules, RuleBlockDns,
						  L"Veyon Internet Access Control - Block External DNS",
						  NET_FW_ACTION_BLOCK, NET_FW_IP_PROTOCOL_UDP,
						  L"53", remoteRange );
	}

	// Add Whitelist rules if configured
	if ( !config.allowedHosts.trimmed().isEmpty() )
	{
		const auto hosts = config.allowedHosts.split( QRegularExpression( "[,;\\s]+" ), Qt::SkipEmptyParts );
		int index = 0;
		for ( const auto& host : hosts )
		{
			QHostInfo hostInfo = QHostInfo::fromName( host.trimmed() );
			for ( const auto& addr : hostInfo.addresses() )
			{
				const auto addrStr = addr.toString().toStdWString();
				const auto ruleName = QString( "Veyon_IAC_Allow_Host_%1" ).arg( ++index ).toStdWString();

				createAndAddRule( pRules, ruleName.c_str(),
								  L"Veyon Internet Access Control - Allowed Host Exception",
								  NET_FW_ACTION_ALLOW, NET_FW_IP_PROTOCOL_ANY,
								  nullptr, addrStr.c_str() );
			}
		}
	}

	pRules->Release();
	pPolicy->Release();

	return SUCCEEDED( hr );
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

	removeRuleByName( pRules, RuleBlockWebTcp );
	removeRuleByName( pRules, RuleBlockWebUdp );
	removeRuleByName( pRules, RuleBlockAllNonLan );
	removeRuleByName( pRules, RuleBlockDns );

	// Remove whitelist host exceptions
	for ( int i = 1; i <= 64; ++i )
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
	const QString remoteRange = QString::fromWCharArray( DefaultInternetRanges );

	if ( config.mode == BlockMode::WebPortsOnly )
	{
		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_Web_TCP"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=%1").arg(config.blockedPorts),
			  QStringLiteral("remoteip=%1").arg(remoteRange), QStringLiteral("enable=yes") } );

		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_Web_UDP"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=443,80"),
			  QStringLiteral("remoteip=%1").arg(remoteRange), QStringLiteral("enable=yes") } );
	}
	else
	{
		QProcess::execute( QStringLiteral("netsh"),
			{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
			  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_All_NonLAN"),
			  QStringLiteral("dir=out"), QStringLiteral("action=block"),
			  QStringLiteral("remoteip=%1").arg(remoteRange), QStringLiteral("enable=yes") } );
	}

	return true;
}

bool WindowsFirewallController::unblockViaNetsh()
{
	QProcess::execute( QStringLiteral("netsh"),
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
		  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_Web_TCP") } );

	QProcess::execute( QStringLiteral("netsh"),
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
		  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_Web_UDP") } );

	QProcess::execute( QStringLiteral("netsh"),
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
		  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_All_NonLAN") } );

	QProcess::execute( QStringLiteral("netsh"),
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
		  QStringLiteral("rule"), QStringLiteral("name=Veyon_IAC_Block_DNS") } );

	return true;
}
