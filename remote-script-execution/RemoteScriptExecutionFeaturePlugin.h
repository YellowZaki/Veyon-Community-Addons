/*
 * RemoteScriptExecutionFeaturePlugin.h - feature plugin declaration
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QPointer>
#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "RemoteScriptExecutionConfiguration.h"
#include "ScriptRunner.h"

class RemoteScriptDialog;

class RemoteScriptExecutionFeaturePlugin : public QObject,
										   public FeatureProviderInterface,
										   public PluginInterface,
										   public ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA( IID "io.veyon.Veyon.Plugins.RemoteScriptExecution" )
	Q_INTERFACES( PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface )

public:
	enum class FeatureCommand
	{
		ExecuteScript = 1,
		ExecutionResult = 2
	};
	Q_ENUM( FeatureCommand )

	enum class Argument
	{
		ExecutionId = 1,
		PackageData,
		Interpreter,
		RunAsSystem,
		TimeoutSeconds,
		Cleanup,
		ScriptArguments,
		ExitCode,
		Output,
		Success,
		ExecutionTimeMs,
		ErrorMessage
	};
	Q_ENUM( Argument )

	explicit RemoteScriptExecutionFeaturePlugin( QObject* parent = nullptr );
	~RemoteScriptExecutionFeaturePlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("e1f98bc4-72de-4ba6-8699-24231b6f0001") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("RemoteScriptExecution");
	}

	QString description() const override
	{
		return tr( "Execute scripts and deploy packages as Administrator silently" );
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

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	ConfigurationPage* createConfigurationPage() override;

	void dispatchExecution( const ComputerControlInterfaceList& computers,
							const QUuid& executionId,
							const QByteArray& packageData,
							const ScriptRunner::ExecutionOptions& options );

private:
	RemoteScriptExecutionConfiguration m_configuration;
	const Feature m_remoteScriptFeature;
	const FeatureList m_features;
	QPointer<RemoteScriptDialog> m_activeDialog;
};
