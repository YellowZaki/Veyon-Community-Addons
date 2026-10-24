/*
 * ScreenRecorderFeaturePlugin.h - Screen Recorder feature plugin declaration
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QPointer>
#include "ConfigurationPagePluginInterface.h"
#include "FeatureProviderInterface.h"
#include "ScreenRecorderConfiguration.h"

class ScreenRecorderControlWidget;
class ScreenRecorderEncodingWorker;

class ScreenRecorderFeaturePlugin : public QObject,
									public FeatureProviderInterface,
									public PluginInterface,
									public ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA( IID "io.veyon.Veyon.Plugins.ScreenRecorder" )
	Q_INTERFACES( PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface )

public:
	enum class FeatureCommand
	{
		NotifyStartRecording,
		NotifyStopRecording
	};
	Q_ENUM( FeatureCommand )

	explicit ScreenRecorderFeaturePlugin( QObject* parent = nullptr );
	~ScreenRecorderFeaturePlugin() override;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral( "c461daa1-f353-425e-8839-5c041d9a3c31" ) };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral( "ScreenRecorder" );
	}

	QString description() const override
	{
		return tr( "Record screens of selected computers to video files (MP4/AVI)" );
	}

	QString vendor() const override
	{
		return QStringLiteral( "Veyon Community" );
	}

	QString copyright() const override
	{
		return QStringLiteral( "Veyon Community Addons" );
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool stopFeature( VeyonMasterInterface& master, const Feature& feature,
					  const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	ConfigurationPage* createConfigurationPage() override;

private Q_SLOTS:
	void onStopRecordingRequested();

private:
	struct RecordingSession
	{
		ComputerControlInterface::Pointer controlInterface;
		ScreenRecorderEncodingWorker* worker{nullptr};
		QTimer* captureTimer{nullptr};
		ComputerControlInterface::UpdateMode originalUpdateMode{ComputerControlInterface::UpdateMode::Monitoring};
	};

	bool startRecording( VeyonMasterInterface* master,
						 const ComputerControlInterfaceList& computerControlInterfaces );
	bool stopRecording();

	QString generateOutputFilePath( const QString& baseDir,
								   const ComputerControlInterface::Pointer& controlInterface,
								   const QString& extension ) const;

	ScreenRecorderConfiguration m_configuration;
	const Feature m_screenRecorderFeature;
	const FeatureList m_features;

	bool m_isRecording{false};
	QPointer<ScreenRecorderControlWidget> m_controlWidget;
	VeyonMasterInterface* m_master{nullptr};
	QMap<ComputerControlInterface*, QSharedPointer<RecordingSession>> m_activeSessions;
};
