/*
 * ScreenRecorderFeaturePlugin.cpp - Screen Recorder feature plugin implementation
 *
 * Part of Veyon Community Addons
 */

#include "ScreenRecorderFeaturePlugin.h"
#include "ScreenRecorderConfigurationPage.h"
#include "ScreenRecorderControlWidget.h"
#include "ScreenRecorderEncodingWorker.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "Logger.h"
#include "PlatformCoreFunctions.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"

#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTimer>

static QString sanitizeFilename( const QString& name )
{
	QString sanitized = name;
	static const QRegularExpression regex( QStringLiteral( "[\\\\/:*?\"<>|]" ) );
	sanitized.replace( regex, QStringLiteral( "_" ) );
	return sanitized.trimmed();
}

ScreenRecorderFeaturePlugin::ScreenRecorderFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_configuration( &VeyonCore::config() ),
	m_screenRecorderFeature(
		QStringLiteral( "ScreenRecorder" ),
		Feature::Flag::Mode | Feature::Flag::AllComponents,
		Feature::Uid( "73fa9068-a631-4389-9ec6-dca65becd3b2" ),
		Feature::Uid(),
		tr( "Record" ),
		tr( "Stop recording" ),
		tr( "Click this button to record the screens of all users into video files." ),
		QStringLiteral( ":/screenrecorder/resources/media-record.png" ) ),
	m_features( { m_screenRecorderFeature } )
{
}

ScreenRecorderFeaturePlugin::~ScreenRecorderFeaturePlugin()
{
	stopRecording();
	delete m_controlWidget;
}

bool ScreenRecorderFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
											   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if ( feature.uid() != m_screenRecorderFeature.uid() )
	{
		return false;
	}

	ComputerControlInterfaceList targets = computerControlInterfaces;
	if ( targets.isEmpty() )
	{
		targets = master.filteredComputerControlInterfaces();
	}
	if ( targets.isEmpty() )
	{
		targets = master.allComputerControlInterfaces();
	}

	if ( targets.isEmpty() )
	{
		QMessageBox::warning( master.mainWindow(),
							  tr( "Screen recording" ),
							  tr( "No computers available to record." ) );
		return false;
	}

	return startRecording( &master, targets );
}

bool ScreenRecorderFeaturePlugin::stopFeature( VeyonMasterInterface& master, const Feature& feature,
											  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED( master )
	Q_UNUSED( computerControlInterfaces )

	if ( feature.uid() != m_screenRecorderFeature.uid() )
	{
		return false;
	}

	return stopRecording();
}

bool ScreenRecorderFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
												 const QVariantMap& arguments,
												 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED( arguments )

	if ( featureUid != m_screenRecorderFeature.uid() )
	{
		return false;
	}

	if ( operation == Operation::Start )
	{
		return startRecording( nullptr, computerControlInterfaces );
	}
	else if ( operation == Operation::Stop )
	{
		return stopRecording();
	}

	return false;
}

QString ScreenRecorderFeaturePlugin::generateOutputFilePath( const QString& baseDir,
															const ComputerControlInterface::Pointer& controlInterface,
															const QString& extension ) const
{
	QString compName = controlInterface->computerName().trimmed();
	if ( compName.isEmpty() )
	{
		compName = controlInterface->computer().displayName().trimmed();
	}
	if ( compName.isEmpty() )
	{
		compName = controlInterface->computer().hostAddress().toString().trimmed();
	}
	if ( compName.isEmpty() )
	{
		compName = QStringLiteral( "Computer" );
	}

	const QString sanitizedComp = sanitizeFilename( compName );
	const QString timestamp = QDateTime::currentDateTime().toString( QStringLiteral( "yyyyMMdd_hhmmss" ) );

	QString targetDir = baseDir;
	if ( m_configuration.saveToComputerSubdirectories() )
	{
		targetDir = baseDir + QLatin1Char( '/' ) + sanitizedComp;
	}

	QDir().mkpath( targetDir );

	return targetDir + QLatin1Char( '/' ) + sanitizedComp + QLatin1Char( '_' ) + timestamp + extension;
}

bool ScreenRecorderFeaturePlugin::startRecording( VeyonMasterInterface* master,
												 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if ( m_isRecording )
	{
		vWarning() << "Screen recording is already active";
		return false;
	}

	m_master = master;
	const QString baseDir = ScreenRecorderUtil::resolvedOutputDirectory( m_configuration );
	const QString extension = ( m_configuration.format() == 0 )
		? QStringLiteral( ".mp4" )
		: QStringLiteral( ".avi" );

	vDebug() << "Starting screen recording for" << computerControlInterfaces.size()
			 << "computer(s), output dir:" << baseDir;

	m_activeSessions.clear();

	const int fps = qBound( 1, m_configuration.framesPerSecond(), 60 );
	const int intervalMs = 1000 / fps;

	for ( const auto& controlInterface : computerControlInterfaces )
	{
		if ( !controlInterface )
		{
			continue;
		}

		auto session = QSharedPointer<RecordingSession>::create();
		session->controlInterface = controlInterface;
		session->originalUpdateMode = controlInterface->updateMode();

		ScreenRecorderEncodingWorker::Options opts;
		opts.filePath = generateOutputFilePath( baseDir, controlInterface, extension );
		opts.computerName = controlInterface->computerName();
		if ( opts.computerName.isEmpty() )
		{
			opts.computerName = controlInterface->computer().displayName();
		}
		if ( opts.computerName.isEmpty() )
		{
			opts.computerName = controlInterface->computer().hostAddress().toString();
		}
		opts.format = static_cast<ScreenRecorderEncodingWorker::Format>( m_configuration.format() );
		opts.fps = fps;
		opts.bitrateKbps = m_configuration.bitrate();
		opts.useFixedResolution = m_configuration.useFixedResolution();
		opts.resolutionWidth = m_configuration.resolutionWidth();
		opts.resolutionHeight = m_configuration.resolutionHeight();
		opts.keepAspectRatio = m_configuration.keepAspectRatio();

		session->worker = new ScreenRecorderEncodingWorker( opts, this );

		// Hook up capture timer to sample frames at regular intervals
		session->captureTimer = new QTimer( this );
		session->captureTimer->setInterval( intervalMs );

		auto rawInterface = controlInterface.data();
		auto rawWorker = session->worker;

		connect( session->captureTimer, &QTimer::timeout, this, [rawInterface, rawWorker]() {
			if ( rawInterface && rawInterface->hasValidFramebuffer() )
			{
				rawWorker->enqueueFrame( rawInterface->framebuffer() );
			}
		} );

		// Switch student connection to Live update mode for smooth recording
		controlInterface->setUpdateMode( ComputerControlInterface::UpdateMode::Live );

		session->worker->start();
		session->captureTimer->start();

		m_activeSessions.insert( rawInterface, session );
	}

	if ( m_activeSessions.isEmpty() )
	{
		vWarning() << "No valid computers found to record";
		return false;
	}

	m_isRecording = true;

	if ( m_configuration.notifyUsers() )
	{
		FeatureMessage msg{ m_screenRecorderFeature.uid(), FeatureCommand::NotifyStartRecording };
		sendFeatureMessage( msg, computerControlInterfaces );
	}

	// Show control widget in Master
	if ( master && master->mainWindow() )
	{
		if ( !m_controlWidget )
		{
			m_controlWidget = new ScreenRecorderControlWidget( master->mainWindow() );
			connect( m_controlWidget, &ScreenRecorderControlWidget::stopRecordingRequested,
					 this, &ScreenRecorderFeaturePlugin::onStopRecordingRequested );
		}
		m_controlWidget->start( m_activeSessions.size(), baseDir );
	}

	return true;
}

bool ScreenRecorderFeaturePlugin::stopRecording()
{
	if ( !m_isRecording )
	{
		return false;
	}

	vDebug() << "Stopping screen recording on" << m_activeSessions.size() << "sessions";

	if ( m_configuration.notifyUsers() )
	{
		ComputerControlInterfaceList interfaces;
		for ( const auto& session : m_activeSessions )
		{
			interfaces.append( session->controlInterface );
		}
		FeatureMessage msg{ m_screenRecorderFeature.uid(), FeatureCommand::NotifyStopRecording };
		sendFeatureMessage( msg, interfaces );
	}

	for ( auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it )
	{
		auto session = it.value();
		if ( session->captureTimer )
		{
			session->captureTimer->stop();
			session->captureTimer->deleteLater();
			session->captureTimer = nullptr;
		}

		if ( session->controlInterface )
		{
			session->controlInterface->setUpdateMode( session->originalUpdateMode );
		}

		if ( session->worker )
		{
			session->worker->stopRecording();
			session->worker->deleteLater();
			session->worker = nullptr;
		}
	}

	m_activeSessions.clear();
	m_isRecording = false;

	if ( m_controlWidget )
	{
		m_controlWidget->stop();
	}

	return true;
}

void ScreenRecorderFeaturePlugin::onStopRecordingRequested()
{
	stopRecording();
}

bool ScreenRecorderFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
													   const MessageContext& messageContext,
													   const FeatureMessage& message )
{
	Q_UNUSED( messageContext )

	if ( message.featureUid() == m_screenRecorderFeature.uid() )
	{
		// Forward message to session worker on student computer
		server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
		return true;
	}

	return false;
}

bool ScreenRecorderFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker,
													   const FeatureMessage& message )
{
	Q_UNUSED( worker )

	if ( message.featureUid() == m_screenRecorderFeature.uid() )
	{
		const auto cmd = message.command<FeatureCommand>();
		if ( cmd == FeatureCommand::NotifyStartRecording )
		{
			auto msgBox = new QMessageBox(
				QMessageBox::Information,
				tr( "Screen recording" ),
				tr( "The screen is now recorded in a video file on the teacher's computer." ) );
			msgBox->setAttribute( Qt::WA_DeleteOnClose );
			msgBox->show();
			VeyonCore::platform().coreFunctions().raiseWindow( msgBox, true );
			QTimer::singleShot( 5000, msgBox, &QWidget::close );
			return true;
		}
		else if ( cmd == FeatureCommand::NotifyStopRecording )
		{
			auto msgBox = new QMessageBox(
				QMessageBox::Information,
				tr( "Screen recording" ),
				tr( "The screen recording on the teacher's computer has been stopped." ) );
			msgBox->setAttribute( Qt::WA_DeleteOnClose );
			msgBox->show();
			VeyonCore::platform().coreFunctions().raiseWindow( msgBox, true );
			QTimer::singleShot( 5000, msgBox, &QWidget::close );
			return true;
		}
	}

	return false;
}

bool ScreenRecorderFeaturePlugin::isFeatureActive( VeyonServerInterface& server,
												  Feature::Uid featureUid ) const
{
	Q_UNUSED( server )

	if ( featureUid == m_screenRecorderFeature.uid() )
	{
		return m_isRecording;
	}

	return false;
}

ConfigurationPage* ScreenRecorderFeaturePlugin::createConfigurationPage()
{
	return new ScreenRecorderConfigurationPage( m_configuration );
}

IMPLEMENT_CONFIG_PROXY( ScreenRecorderConfiguration )
