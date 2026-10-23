/*
 * InternetAccessControlFeaturePlugin.cpp - implementation of InternetAccessControlFeaturePlugin
 *
 * Part of Veyon Community Addons
 */

#include <QCoreApplication>

#include "InternetAccessControlFeaturePlugin.h"
#include "InternetAccessControlConfigurationPage.h"
#include "WindowsFirewallController.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "Logger.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"

InternetAccessControlFeaturePlugin::InternetAccessControlFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_configuration( &VeyonCore::config() ),
	m_internetAccessControlFeature(
		QStringLiteral( "InternetAccessControl" ),
		Feature::Flag::Mode | Feature::Flag::AllComponents,
		Feature::Uid( "b7218ac0-4592-4e4b-a256-d71fa9e31002" ),
		Feature::Uid(),
		tr( "Block Internet" ),
		tr( "Unblock Internet" ),
		tr( "Block or allow internet access on selected computers while keeping local network access active." ),
		QStringLiteral( ":/internetaccesscontrol/resources/internet-web.png" ) ),
	m_features( { m_internetAccessControlFeature } )
{
	if ( VeyonCore::component() == VeyonCore::Component::Service ||
		 VeyonCore::component() == VeyonCore::Component::Server )
	{
		connect( VeyonCore::instance(), &VeyonCore::initialized, this, [this]() {
			if ( m_configuration.autoUnblockOnExit() )
			{
				vDebug() << "Component initialized: performing startup firewall cleanup";
				WindowsFirewallController::instance().cleanUp();
			}
		} );
	}
}

InternetAccessControlFeaturePlugin::~InternetAccessControlFeaturePlugin()
{
	if ( m_configuration.autoUnblockOnExit() )
	{
		WindowsFirewallController::instance().cleanUp();
	}
}

bool InternetAccessControlFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
														 const QVariantMap& arguments,
														 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED( arguments )

	if ( hasFeature( featureUid ) == false )
	{
		return false;
	}

	if ( operation == Operation::Start )
	{
		vDebug() << "Starting Internet Access block on" << computerControlInterfaces.size() << "computer(s)";

		const FeatureMessage message{ featureUid, FeatureCommand::BlockInternet };
		for ( const auto& controlInterface : computerControlInterfaces )
		{
			if ( controlInterface )
			{
				controlInterface->sendFeatureMessage( message );
			}
		}

		return true;
	}

	if ( operation == Operation::Stop )
	{
		vDebug() << "Stopping Internet Access block on" << computerControlInterfaces.size() << "computer(s)";

		const FeatureMessage message{ featureUid, FeatureCommand::UnblockInternet };
		for ( const auto& controlInterface : computerControlInterfaces )
		{
			if ( controlInterface )
			{
				controlInterface->sendFeatureMessage( message );
			}
		}

		return true;
	}

	return false;
}

bool InternetAccessControlFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
															   const MessageContext& messageContext,
															   const FeatureMessage& message )
{
	Q_UNUSED( server )
	Q_UNUSED( messageContext )

	if ( message.featureUid() == m_internetAccessControlFeature.uid() )
	{
		if ( message.command<FeatureCommand>() == FeatureCommand::UnblockInternet )
		{
			vDebug() << "Server received UnblockInternet command";
			WindowsFirewallController::instance().unblockInternet();
			return true;
		}

		if ( message.command<FeatureCommand>() == FeatureCommand::BlockInternet )
		{
			vDebug() << "Server received BlockInternet command, applying firewall rules directly";

			WindowsFirewallController::Config config;
			if ( message.hasArgument( Argument::BlockMode ) )
			{
				config.mode = static_cast<WindowsFirewallController::BlockMode>(
					message.argument( Argument::BlockMode ).toInt() );
			}
			else
			{
				config.mode = static_cast<WindowsFirewallController::BlockMode>( m_configuration.blockMode() );
			}

			if ( message.hasArgument( Argument::BlockedPorts ) )
			{
				config.blockedPorts = message.argument( Argument::BlockedPorts ).toString();
			}
			else
			{
				config.blockedPorts = m_configuration.blockedPorts();
			}

			if ( config.blockedPorts.isEmpty() )
			{
				config.blockedPorts = QStringLiteral( "80,443,8080,8443" );
			}

			if ( message.hasArgument( Argument::AllowedSubnets ) )
			{
				config.allowedSubnets = message.argument( Argument::AllowedSubnets ).toString();
			}
			else
			{
				config.allowedSubnets = m_configuration.allowedSubnets();
			}

			if ( message.hasArgument( Argument::AllowedHosts ) )
			{
				config.allowedHosts = message.argument( Argument::AllowedHosts ).toString();
			}
			else
			{
				config.allowedHosts = m_configuration.allowedHosts();
			}

			if ( message.hasArgument( Argument::BlockDns ) )
			{
				config.blockDns = message.argument( Argument::BlockDns ).toBool();
			}
			else
			{
				config.blockDns = m_configuration.blockDnsTraffic();
			}

			WindowsFirewallController::instance().blockInternet( config );
			return true;
		}
	}

	return false;
}

bool InternetAccessControlFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker,
															   const FeatureMessage& message )
{
	Q_UNUSED( worker )
	Q_UNUSED( message )

	return false;
}

bool InternetAccessControlFeaturePlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED( server )

	if ( featureUid == m_internetAccessControlFeature.uid() )
	{
		return WindowsFirewallController::instance().isInternetBlocked();
	}

	return false;
}

ConfigurationPage* InternetAccessControlFeaturePlugin::createConfigurationPage()
{
	return new InternetAccessControlConfigurationPage( m_configuration );
}

IMPLEMENT_CONFIG_PROXY(InternetAccessControlConfiguration)
