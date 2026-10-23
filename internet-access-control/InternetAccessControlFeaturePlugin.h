/*
 * InternetAccessControlFeaturePlugin.h - declaration of InternetAccessControlFeaturePlugin
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "InternetAccessControlConfiguration.h"

class InternetAccessControlFeaturePlugin : public QObject,
										   public FeatureProviderInterface,
										   public PluginInterface,
										   public ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA( IID "io.veyon.Veyon.Plugins.InternetAccessControl" )
	Q_INTERFACES( PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface )

public:
	enum class Argument
	{
		BlockMode,
		BlockedPorts,
		AllowedSubnets,
		AllowedHosts,
		BlockDns
	};
	Q_ENUM( Argument )

	explicit InternetAccessControlFeaturePlugin( QObject* parent = nullptr );
	~InternetAccessControlFeaturePlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("e82fa510-39b1-419a-9e11-8845e2c56a01") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("InternetAccessControl");
	}

	QString description() const override
	{
		return tr( "Block or restore internet access on student computers" );
	}

	QString vendor() const override
	{
		return QStringLiteral("Veyon Community");
	}

	QString copyright() const override
	{
		return QStringLiteral("Veyon Community Addons");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	ConfigurationPage* createConfigurationPage() override;

private:
	enum class FeatureCommand
	{
		BlockInternet,
		UnblockInternet
	};

	InternetAccessControlConfiguration m_configuration;
	const Feature m_internetAccessControlFeature;
	const FeatureList m_features;
};
