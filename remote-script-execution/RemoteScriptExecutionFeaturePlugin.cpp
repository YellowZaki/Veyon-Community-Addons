/*
 * RemoteScriptExecutionFeaturePlugin.cpp - implementation of RemoteScriptExecutionFeaturePlugin
 *
 * Part of Veyon Community Addons
 */

#include "RemoteScriptExecutionFeaturePlugin.h"
#include "RemoteScriptExecutionConfigurationPage.h"
#include "RemoteScriptDialog.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"
#include "VeyonCore.h"

#include <QIODevice>
#include <QThreadPool>

RemoteScriptExecutionFeaturePlugin::RemoteScriptExecutionFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_configuration( &VeyonCore::config() ),
	m_remoteScriptFeature( QStringLiteral("RemoteScriptExecution"),
						  Feature::Flag::Action | Feature::Flag::AllComponents,
						  Feature::Uid( "d56a3371-3c94-44b2-9387-9bb329c0f992" ),
						  Feature::Uid(),
						  tr( "Run Script / Deploy" ), {},
						  tr( "Execute scripts and deploy packages as Administrator silently" ),
						  QStringLiteral(":/remotescriptexecution/resources/remote-script-execution.png") ),
	m_features( { m_remoteScriptFeature } )
{
}

RemoteScriptExecutionFeaturePlugin::~RemoteScriptExecutionFeaturePlugin()
{
}

bool RemoteScriptExecutionFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
														const QVariantMap& arguments,
														const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED( featureUid )
	Q_UNUSED( operation )
	Q_UNUSED( arguments )
	Q_UNUSED( computerControlInterfaces )
	return false;
}

bool RemoteScriptExecutionFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
													   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if ( feature == m_remoteScriptFeature )
	{
		if ( m_activeDialog )
		{
			m_activeDialog->close();
		}

		m_activeDialog = new RemoteScriptDialog( this, computerControlInterfaces, master.mainWindow() );
		m_activeDialog->setAttribute( Qt::WA_DeleteOnClose );
		m_activeDialog->show();
		return true;
	}

	return false;
}

void RemoteScriptExecutionFeaturePlugin::dispatchExecution( const ComputerControlInterfaceList& computers,
															const QUuid& executionId,
															const QByteArray& packageData,
															const ScriptRunner::ExecutionOptions& options )
{
	FeatureMessage msg( m_remoteScriptFeature.uid(), FeatureCommand::ExecuteScript );
	msg.addArgument( Argument::ExecutionId, executionId.toString() );
	msg.addArgument( Argument::PackageData, packageData );
	msg.addArgument( Argument::Interpreter, static_cast<int>( options.interpreter ) );
	msg.addArgument( Argument::RunAsSystem, options.runAsSystem );
	msg.addArgument( Argument::TimeoutSeconds, options.timeoutSeconds );
	msg.addArgument( Argument::Cleanup, options.cleanupAfter );
	msg.addArgument( Argument::ScriptArguments, options.arguments );

	for ( const auto& computer : computers )
	{
		computer->sendFeatureMessage( msg );
	}
}

bool RemoteScriptExecutionFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
															   const MessageContext& messageContext,
															   const FeatureMessage& message )
{
	if ( message.featureUid() != m_remoteScriptFeature.uid() )
	{
		return false;
	}

	if ( message.command<FeatureCommand>() == FeatureCommand::ExecuteScript )
	{
		QString execIdStr = message.argument( Argument::ExecutionId ).toString();
		QUuid executionId = QUuid::fromString( execIdStr );
		QByteArray packageData = message.argument( Argument::PackageData ).toByteArray();

		ScriptRunner::ExecutionOptions options;
		options.interpreter = static_cast<ScriptRunner::InterpreterType>( message.argument( Argument::Interpreter ).toInt() );
		options.runAsSystem = message.argument( Argument::RunAsSystem ).toBool();
		options.timeoutSeconds = message.argument( Argument::TimeoutSeconds ).toInt();
		options.cleanupAfter = message.argument( Argument::Cleanup ).toBool();
		options.arguments = message.argument( Argument::ScriptArguments ).toString();

		// Run asynchronously in thread pool to prevent blocking the Veyon server event loop
		QThreadPool::globalInstance()->start( [&server, messageContext, packageData, options, executionId, featureUid = m_remoteScriptFeature.uid()]() {
			ScriptPackage package;
			package.deserializeAndDecompress( packageData );

			auto result = ScriptRunner::execute( package, options );

			FeatureMessage reply( featureUid, FeatureCommand::ExecutionResult );
			reply.addArgument( Argument::ExecutionId, executionId.toString() );
			reply.addArgument( Argument::ExitCode, result.exitCode );
			reply.addArgument( Argument::Output, result.output );
			reply.addArgument( Argument::Success, result.success );
			reply.addArgument( Argument::ExecutionTimeMs, result.executionTimeMs );
			reply.addArgument( Argument::ErrorMessage, result.errorMessage );

			if ( messageContext.ioDevice() && messageContext.ioDevice()->thread() )
			{
				QMetaObject::invokeMethod( messageContext.ioDevice(), [&server, messageContext, reply]() {
					server.sendFeatureMessageReply( messageContext, reply );
				}, Qt::QueuedConnection );
			}
		} );

		return true;
	}

	return false;
}

bool RemoteScriptExecutionFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
															   const FeatureMessage& message )
{
	if ( message.featureUid() != m_remoteScriptFeature.uid() )
	{
		return false;
	}

	if ( message.command<FeatureCommand>() == FeatureCommand::ExecutionResult )
	{
		QUuid executionId = QUuid::fromString( message.argument( Argument::ExecutionId ).toString() );
		int exitCode = message.argument( Argument::ExitCode ).toInt();
		QString output = message.argument( Argument::Output ).toString();
		bool success = message.argument( Argument::Success ).toBool();
		qint64 executionTimeMs = message.argument( Argument::ExecutionTimeMs ).toLongLong();
		QString errorMessage = message.argument( Argument::ErrorMessage ).toString();

		if ( m_activeDialog )
		{
			m_activeDialog->handleExecutionResult( computerControlInterface,
												   executionId,
												   exitCode,
												   output,
												   success,
												   executionTimeMs,
												   errorMessage );
		}

		return true;
	}

	return false;
}

bool RemoteScriptExecutionFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED( worker )
	Q_UNUSED( message )
	return false;
}

bool RemoteScriptExecutionFeaturePlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED( server )
	Q_UNUSED( featureUid )
	return false;
}

ConfigurationPage* RemoteScriptExecutionFeaturePlugin::createConfigurationPage()
{
	return new RemoteScriptExecutionConfigurationPage( m_configuration );
}
