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
	m_configuration(),
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
	if ( VeyonCore::component() == VeyonCore::Component::Service )
	{
		connect( VeyonCore::instance(), &VeyonCore::initialized, this, [this]() {
			if ( m_configuration.autoUnblockOnExit() )
			{
				vDebug() << "Service initialized: performing startup firewall cleanup";
				WindowsFirewallController::instance().cleanUp();
			}
		} );
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

		FeatureMessage message{ featureUid, FeatureCommand::BlockInternet };
		message.addArgument( argToString( Argument::BlockMode ), m_configuration.blockMode() );
		message.addArgument( argToString( Argument::BlockedPorts ), m_configuration.blockedPorts() );
		message.addArgument( argToString( Argument::AllowedSubnets ), m_configuration.allowedSubnets() );
		message.addArgument( argToString( Argument::AllowedHosts ), m_configuration.allowedHosts() );
		message.addArgument( argToString( Argument::BlockDns ), m_configuration.blockDnsTraffic() );

		sendFeatureMessage( message, computerControlInterfaces );
		return true;
	}

	if ( operation == Operation::Stop )
	{
		vDebug() << "Stopping Internet Access block on" << computerControlInterfaces.size() << "computer(s)";

		FeatureMessage message{ featureUid, FeatureCommand::UnblockInternet };
		sendFeatureMessage( message, computerControlInterfaces );
		return true;
	}

	return false;
}

bool InternetAccessControlFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
															   const MessageContext& messageContext,
															   const FeatureMessage& message )
{
	Q_UNUSED( messageContext )

	if ( message.featureUid() == m_internetAccessControlFeature.uid() )
	{
		if ( message.command<FeatureCommand>() == FeatureCommand::UnblockInternet )
		{
			vDebug() << "Server received UnblockInternet command";
			WindowsFirewallController::instance().unblockInternet();

			if ( server.featureWorkerManager().isWorkerRunning( message.featureUid() ) )
			{
				server.featureWorkerManager().sendMessageToManagedSystemWorker( message );
			}
			return true;
		}

		if ( message.command<FeatureCommand>() == FeatureCommand::BlockInternet )
		{
			vDebug() << "Server received BlockInternet command, delegating to system worker";
			server.featureWorkerManager().sendMessageToManagedSystemWorker( message );
			return true;
		}
	}

	return false;
}

bool InternetAccessControlFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker,
															   const FeatureMessage& message )
{
	Q_UNUSED( worker )

	if ( message.featureUid() == m_internetAccessControlFeature.uid() )
	{
		switch ( message.command<FeatureCommand>() )
		{
		case FeatureCommand::BlockInternet:
		{
			WindowsFirewallController::Config config;
			const auto args = message.arguments();

			config.mode = static_cast<WindowsFirewallController::BlockMode>(
				args.value( argToString( Argument::BlockMode ), 0 ).toInt() );
			config.blockedPorts = args.value( argToString( Argument::BlockedPorts ), QStringLiteral( "80,443,8080,8443" ) ).toString();
			config.allowedSubnets = args.value( argToString( Argument::AllowedSubnets ), QString() ).toString();
			config.allowedHosts = args.value( argToString( Argument::AllowedHosts ), QString() ).toString();
			config.blockDns = args.value( argToString( Argument::BlockDns ), false ).toBool();

			vDebug() << "Worker applying firewall block rules...";
			WindowsFirewallController::instance().blockInternet( config );
			return true;
		}

		case FeatureCommand::UnblockInternet:
		{
			vDebug() << "Worker removing firewall block rules...";
			WindowsFirewallController::instance().unblockInternet();
			QCoreApplication::quit();
			return true;
		}

		default:
			break;
		}
	}

	return false;
}

bool InternetAccessControlFeaturePlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	if ( featureUid == m_internetAccessControlFeature.uid() )
	{
		return WindowsFirewallController::instance().isInternetBlocked() ||
			   server.featureWorkerManager().isWorkerRunning( featureUid );
	}

	return false;
}

ConfigurationPage* InternetAccessControlFeaturePlugin::createConfigurationPage()
{
	return new InternetAccessControlConfigurationPage( m_configuration );
}
