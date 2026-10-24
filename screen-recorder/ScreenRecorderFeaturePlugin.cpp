/*
 * ScreenRecorderFeaturePlugin.cpp - Screen Recorder feature plugin implementation
 *
 * Part of Veyon Community Addons
 */

#include "ScreenRecorderFeaturePlugin.h"
#include "ScreenRecorderConfigurationPage.h"
#include "ScreenRecorderEncodingWorker.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "Logger.h"
#include "PlatformCoreFunctions.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>
#include <QToolButton>

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
	m_buttonUpdateTimer = new QTimer( this );
	m_buttonUpdateTimer->setInterval( 1000 );
	connect( m_buttonUpdateTimer, &QTimer::timeout, this, &ScreenRecorderFeaturePlugin::updateToolbarButton );
}

ScreenRecorderFeaturePlugin::~ScreenRecorderFeaturePlugin()
{
	stopRecording();
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

	if ( feature.uid() != m_screenRecorderFeature.uid() )
	{
		return false;
	}

	return stopRecording( computerControlInterfaces );
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
		return stopRecording( computerControlInterfaces );
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
	if ( master )
	{
		m_master = master;
	}

	const QString baseDir = ScreenRecorderUtil::resolvedOutputDirectory( m_configuration );
	const QString extension = QStringLiteral( ".mp4" );

	const int fps = qBound( 1, m_configuration.framesPerSecond(), 60 );
	const int intervalMs = 1000 / fps;

	ComputerControlInterfaceList newlyStartedInterfaces;

	for ( const auto& controlInterface : computerControlInterfaces )
	{
		if ( !controlInterface )
		{
			continue;
		}

		if ( m_activeSessions.contains( controlInterface.data() ) )
		{
			// Already actively recording this computer
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
		opts.quality = m_configuration.quality();
		opts.useFixedResolution = m_configuration.useFixedResolution();
		opts.resolutionWidth = m_configuration.resolutionWidth();
		opts.resolutionHeight = m_configuration.resolutionHeight();
		opts.keepAspectRatio = m_configuration.keepAspectRatio();

		session->worker = new ScreenRecorderEncodingWorker( opts, this );

		// Hook up capture timer to sample frames at regular intervals
		session->captureTimer = new QTimer( this );
		session->captureTimer->setInterval( intervalMs );

		QWeakPointer<ComputerControlInterface> weakInterface = controlInterface.toWeakRef();
		auto rawWorker = session->worker;

		connect( session->captureTimer, &QTimer::timeout, this, [weakInterface, rawWorker]() {
			auto iface = weakInterface.toStrongRef();
			if ( iface && iface->hasValidFramebuffer() )
			{
				const QImage frame = iface->framebuffer();
				if ( !frame.isNull() )
				{
					rawWorker->enqueueFrame( frame.copy() );
				}
			}
		} );

		// Switch student connection to Live update mode for smooth recording
		controlInterface->setUpdateMode( ComputerControlInterface::UpdateMode::Live );

		// Add screen recorder feature to activeFeatures so Veyon's item delegate displays the recording badge
		auto features = controlInterface->activeFeatures();
		if ( !features.contains( m_screenRecorderFeature.uid() ) )
		{
			features.append( m_screenRecorderFeature.uid() );
			controlInterface->setActiveFeatures( features );
		}

		session->worker->start();
		session->captureTimer->start();

		m_activeSessions.insert( controlInterface.data(), session );
		newlyStartedInterfaces.append( controlInterface );
	}

	if ( m_activeSessions.isEmpty() )
	{
		vWarning() << "No valid computers found to record";
		return false;
	}

	if ( !m_isRecording )
	{
		m_isRecording = true;
		m_recordingElapsedTimer.start();
		if ( m_buttonUpdateTimer )
		{
			m_buttonUpdateTimer->start();
		}
	}

	if ( m_configuration.notifyUsers() && !newlyStartedInterfaces.isEmpty() )
	{
		FeatureMessage msg{ m_screenRecorderFeature.uid(), FeatureCommand::NotifyStartRecording };
		sendFeatureMessage( msg, newlyStartedInterfaces );
	}

	updateToolbarButton();
	updateComputerOverlays();

	vDebug() << "Screen recording active for" << m_activeSessions.size() << "computer(s), output dir:" << baseDir;

	return true;
}

bool ScreenRecorderFeaturePlugin::stopRecording( const ComputerControlInterfaceList& computerControlInterfaces )
{
	if ( !m_isRecording && m_activeSessions.isEmpty() )
	{
		return false;
	}

	QList<ComputerControlInterface*> toStop;
	if ( computerControlInterfaces.isEmpty() )
	{
		toStop = m_activeSessions.keys();
	}
	else
	{
		for ( const auto& controlInterface : computerControlInterfaces )
		{
			if ( controlInterface && m_activeSessions.contains( controlInterface.data() ) )
			{
				toStop.append( controlInterface.data() );
			}
		}
		// If targets were provided but none matched (e.g. toolbar click passed all filtered computers),
		// stop all active sessions
		if ( toStop.isEmpty() )
		{
			toStop = m_activeSessions.keys();
		}
	}

	vDebug() << "Stopping screen recording on" << toStop.size() << "session(s), remaining:" << ( m_activeSessions.size() - toStop.size() );

	ComputerControlInterfaceList notifiedInterfaces;

	for ( auto* ifacePtr : toStop )
	{
		auto session = m_activeSessions.take( ifacePtr );
		if ( !session )
		{
			continue;
		}

		if ( session->captureTimer )
		{
			session->captureTimer->stop();
			delete session->captureTimer;
			session->captureTimer = nullptr;
		}

		if ( session->controlInterface )
		{
			session->controlInterface->setUpdateMode( session->originalUpdateMode );

			// Remove active feature badge so the red dot disappears from the thumbnail
			auto features = session->controlInterface->activeFeatures();
			features.removeAll( m_screenRecorderFeature.uid() );
			session->controlInterface->setActiveFeatures( features );

			notifiedInterfaces.append( session->controlInterface );
		}

		if ( session->worker )
		{
			session->worker->stopRecording();
			delete session->worker;
			session->worker = nullptr;
		}
	}

	if ( m_configuration.notifyUsers() && !notifiedInterfaces.isEmpty() )
	{
		FeatureMessage msg{ m_screenRecorderFeature.uid(), FeatureCommand::NotifyStopRecording };
		sendFeatureMessage( msg, notifiedInterfaces );
	}

	updateComputerOverlays();

	if ( m_activeSessions.isEmpty() )
	{
		m_isRecording = false;
		if ( m_buttonUpdateTimer )
		{
			m_buttonUpdateTimer->stop();
		}
		m_recordingElapsedTimer.invalidate();
		resetToolbarButton();

		// If master is still in ScreenRecorder mode after stopping (e.g. from context menu),
		// asynchronously switch to monitoring mode so we don't interfere with VeyonMaster::runFeature
		if ( m_master && m_master->mainWindow() )
		{
			QTimer::singleShot( 50, this, [this]() {
				if ( m_activeSessions.isEmpty() && m_master && m_master->mainWindow() )
				{
					auto monitoringBtn = m_master->mainWindow()->findChild<QToolButton*>( QStringLiteral( "MonitoringMode" ) );
					if ( monitoringBtn && !monitoringBtn->isChecked() )
					{
						monitoringBtn->click();
					}
				}
			} );
		}
	}
	else
	{
		updateToolbarButton();
	}

	return true;
}

void ScreenRecorderFeaturePlugin::updateToolbarButton()
{
	if ( !m_master || !m_master->mainWindow() )
	{
		return;
	}

	auto button = m_master->mainWindow()->findChild<QToolButton*>( QStringLiteral( "ScreenRecorder" ) );
	if ( !button )
	{
		return;
	}

	const qint64 elapsedSec = m_recordingElapsedTimer.isValid() ? ( m_recordingElapsedTimer.elapsed() / 1000 ) : 0;
	const int hours = static_cast<int>( elapsedSec / 3600 );
	const int minutes = static_cast<int>( ( elapsedSec % 3600 ) / 60 );
	const int seconds = static_cast<int>( elapsedSec % 60 );

	QString timeStr;
	if ( hours > 0 )
	{
		timeStr = QStringLiteral( "%1:%2:%3" )
					  .arg( hours, 2, 10, QLatin1Char( '0' ) )
					  .arg( minutes, 2, 10, QLatin1Char( '0' ) )
					  .arg( seconds, 2, 10, QLatin1Char( '0' ) );
	}
	else
	{
		timeStr = QStringLiteral( "%1:%2" )
					  .arg( minutes, 2, 10, QLatin1Char( '0' ) )
					  .arg( seconds, 2, 10, QLatin1Char( '0' ) );
	}

	button->setIcon( QIcon( QStringLiteral( ":/screenrecorder/resources/media-record-active.png" ) ) );
	button->setText( QStringLiteral( "%1\n%2" ).arg( tr( "Stop recording" ), timeStr ) );

	// Ensure all active recording sessions maintain their activeFeatures badge
	for ( auto it = m_activeSessions.constBegin(); it != m_activeSessions.constEnd(); ++it )
	{
		const auto& session = it.value();
		if ( session && session->controlInterface )
		{
			auto features = session->controlInterface->activeFeatures();
			if ( !features.contains( m_screenRecorderFeature.uid() ) )
			{
				features.append( m_screenRecorderFeature.uid() );
				session->controlInterface->setActiveFeatures( features );
			}
		}
	}
}

void ScreenRecorderFeaturePlugin::resetToolbarButton()
{
	if ( !m_master || !m_master->mainWindow() )
	{
		return;
	}

	auto button = m_master->mainWindow()->findChild<QToolButton*>( QStringLiteral( "ScreenRecorder" ) );
	if ( button )
	{
		button->setIcon( QIcon( QStringLiteral( ":/screenrecorder/resources/media-record.png" ) ) );
		button->setText( tr( "Record" ) );
	}
}

void ScreenRecorderFeaturePlugin::updateComputerOverlays()
{
	if ( !m_master || !m_master->mainWindow() )
	{
		return;
	}

	const auto views = m_master->mainWindow()->findChildren<QAbstractItemView*>();
	for ( auto view : views )
	{
		if ( view && view->viewport() )
		{
			view->viewport()->update();
		}
	}
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
