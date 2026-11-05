/*
 * ScreenRecorderFeaturePlugin.cpp - Screen Recorder feature plugin implementation
 *
 * Part of Veyon Community Addons
 */

#include "ScreenRecorderFeaturePlugin.h"
#include "ScreenRecorderConfigurationPage.h"
#include "ScreenRecorderEncodingWorker.h"
#include "ComputerControlInterface.h"
#include "ComputerListModel.h"
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
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QStyledItemDelegate>
#include <QToolButton>

class ScreenRecorderItemDelegate : public QStyledItemDelegate
{
public:
	explicit ScreenRecorderItemDelegate( QAbstractItemDelegate* originalDelegate,
	                                     ScreenRecorderFeaturePlugin* plugin,
	                                     QObject* parent = nullptr ) :
		QStyledItemDelegate( parent ),
		m_originalDelegate( originalDelegate ),
		m_plugin( plugin )
	{
	}

	void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override
	{
		if ( m_originalDelegate )
		{
			m_originalDelegate->paint( painter, option, index );
		}
		else
		{
			QStyledItemDelegate::paint( painter, option, index );
		}

		if ( m_plugin )
		{
			m_plugin->drawItemOverlay( painter, option, index );
		}
	}

	QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const override
	{
		if ( m_originalDelegate )
		{
			return m_originalDelegate->sizeHint( option, index );
		}
		return QStyledItemDelegate::sizeHint( option, index );
	}

private:
	QPointer<QAbstractItemDelegate> m_originalDelegate;
	ScreenRecorderFeaturePlugin* m_plugin{nullptr};
};

static QString sanitizeFilename( const QString& name )
{
	QString sanitized = name;
	static const QRegularExpression regex( QStringLiteral( "[\\\\/:*?\"<>|]" ) );
	sanitized.replace( regex, QStringLiteral( "_" ) );
	return sanitized.trimmed();
}

static void purgeOldRecordings( const QString& directory, int maxFiles )
{
	if ( maxFiles <= 0 || directory.isEmpty() )
	{
		return;
	}

	QDir dir( directory );
	if ( !dir.exists() )
	{
		return;
	}

	// Filter for video files (.mp4 and .avi if any old ones remain)
	QFileInfoList files = dir.entryInfoList( { QStringLiteral( "*.mp4" ), QStringLiteral( "*.avi" ) },
											QDir::Files,
											QDir::Time | QDir::Reversed ); // Oldest first

	while ( files.size() >= maxFiles )
	{
		const QFileInfo oldest = files.takeFirst();
		vDebug() << "Purging old recording to maintain limit of" << maxFiles << ":" << oldest.absoluteFilePath();
		QFile::remove( oldest.absoluteFilePath() );
	}
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

	// Always save to computer-specific subdirectories as requested
	const QString targetDir = baseDir + QLatin1Char( '/' ) + sanitizedComp;
	QDir().mkpath( targetDir );

	// Purge oldest recordings if exceeding configured limit
	purgeOldRecordings( targetDir, m_configuration.maxRecordingsPerComputer() );

	return targetDir + QLatin1Char( '/' ) + sanitizedComp + QLatin1Char( '_' ) + timestamp + extension;
}

bool ScreenRecorderFeaturePlugin::startRecording( VeyonMasterInterface* master,
												 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if ( master )
	{
		m_master = master;
	}

	installItemDelegates();

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

		// Keep recording feature registered even if student computer disconnects, turns off, or reconnects
		auto rawInterface = controlInterface.data();
		connect( rawInterface, &ComputerControlInterface::activeFeaturesChanged, this, [this, rawInterface]() {
			maintainActiveFeature( rawInterface );
		} );
		connect( rawInterface, &ComputerControlInterface::stateChanged, this, [this, rawInterface]() {
			onComputerStateChanged( rawInterface );
		} );

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
	if ( computerControlInterfaces.size() == 1 )
	{
		// Context menu right-click: stop only the single targeted computer
		auto* target = computerControlInterfaces.first().data();
		if ( target && m_activeSessions.contains( target ) )
		{
			toStop.append( target );
		}
	}
	else if ( !computerControlInterfaces.isEmpty() )
	{
		for ( const auto& controlInterface : computerControlInterfaces )
		{
			if ( controlInterface && m_activeSessions.contains( controlInterface.data() ) )
			{
				toStop.append( controlInterface.data() );
			}
		}
	}

	// If no specific targets matched (or called with empty list via global stop), stop all active sessions
	if ( toStop.isEmpty() )
	{
		toStop = m_activeSessions.keys();
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

		disconnect( ifacePtr, nullptr, this, nullptr );

		if ( session->captureTimer )
		{
			session->captureTimer->stop();
			delete session->captureTimer;
			session->captureTimer = nullptr;
		}

		if ( session->controlInterface )
		{
			session->controlInterface->setUpdateMode( session->originalUpdateMode );

			// Remove active feature badge so the dot disappears from the thumbnail
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

	// Check for disconnected computers if autoStopOnDisconnect is enabled
	if ( m_configuration.autoStopOnDisconnect() )
	{
		for ( auto* iface : m_activeSessions.keys() )
		{
			if ( iface && iface->state() != ComputerControlInterface::State::Connected )
			{
				vInfo() << "ScreenRecorder: periodic auto-stopping recording for disconnected computer" << iface->computerName();
				const auto session = m_activeSessions.value( iface );
				if ( session && session->controlInterface )
				{
					stopRecording( { session->controlInterface } );
				}
			}
		}
	}

	// Ensure all active recording sessions maintain their activeFeatures badge
	for ( auto* iface : m_activeSessions.keys() )
	{
		maintainActiveFeature( iface );
	}

	installItemDelegates();
	updateComputerOverlays();
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

	installItemDelegates();

	const auto views = m_master->mainWindow()->findChildren<QAbstractItemView*>();
	for ( auto view : views )
	{
		if ( view && view->viewport() )
		{
			view->viewport()->update();
		}
	}
}

void ScreenRecorderFeaturePlugin::installItemDelegates()
{
	if ( !m_master || !m_master->mainWindow() )
	{
		return;
	}

	const auto views = m_master->mainWindow()->findChildren<QAbstractItemView*>();
	for ( auto view : views )
	{
		if ( view && !view->property( "ScreenRecorderDelegateInstalled" ).toBool() )
		{
			auto originalDelegate = view->itemDelegate();
			auto wrapperDelegate = new ScreenRecorderItemDelegate( originalDelegate, this, view );
			view->setItemDelegate( wrapperDelegate );
			view->setProperty( "ScreenRecorderDelegateInstalled", true );
		}
	}
}

void ScreenRecorderFeaturePlugin::maintainActiveFeature( ComputerControlInterface* iface )
{
	if ( m_updatingActiveFeatures || !iface || !m_activeSessions.contains( iface ) )
	{
		return;
	}

	if ( m_configuration.autoStopOnDisconnect() && iface->state() != ComputerControlInterface::State::Connected )
	{
		return;
	}

	auto features = iface->activeFeatures();
	if ( !features.contains( m_screenRecorderFeature.uid() ) )
	{
		m_updatingActiveFeatures = true;
		features.append( m_screenRecorderFeature.uid() );
		iface->setActiveFeatures( features );
		m_updatingActiveFeatures = false;
	}
}

void ScreenRecorderFeaturePlugin::onComputerStateChanged( ComputerControlInterface* iface )
{
	if ( !iface || !m_activeSessions.contains( iface ) )
	{
		return;
	}

	if ( iface->state() != ComputerControlInterface::State::Connected )
	{
		if ( m_configuration.autoStopOnDisconnect() )
		{
			vInfo() << "ScreenRecorder: auto-stopping recording for disconnected computer" << iface->computerName();
			const auto session = m_activeSessions.value( iface );
			if ( session && session->controlInterface )
			{
				stopRecording( { session->controlInterface } );
			}
			return;
		}
	}

	maintainActiveFeature( iface );
	updateComputerOverlays();
}

void ScreenRecorderFeaturePlugin::drawItemOverlay( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index )
{
	if ( !painter || !index.isValid() || !index.model() )
	{
		return;
	}

	// If autoStopOnDisconnect is enabled, the recording terminates on disconnect and no pause overlay is drawn
	if ( m_configuration.autoStopOnDisconnect() )
	{
		return;
	}

	const auto controlInterface = index.model()->data( index, ComputerListModel::ControlInterfaceRole )
	                                  .value<ComputerControlInterface::Pointer>();
	if ( !controlInterface || !m_activeSessions.contains( controlInterface.data() ) )
	{
		return;
	}

	// When connected, Veyon's native drawFeatureIcons already paints the red badge.
	// We paint our orange badge only when the computer is offline/disconnected.
	const bool isOffline = ( controlInterface->state() != ComputerControlInterface::State::Connected );
	if ( !isOffline )
	{
		return;
	}

	const int x = option.rect.topLeft().x() + 8;
	const int y = option.rect.topLeft().y() + 8;
	constexpr int size = 32;

	painter->save();
	painter->setRenderHint( QPainter::Antialiasing );

	// 1. Semi-transparent background pill with vibrant orange border
	painter->setBrush( QColor( 255, 255, 255, 220 ) );
	painter->setPen( QPen( QColor( 235, 120, 20 ), 1.5 ) );
	painter->drawRoundedRect( QRect( x, y, size, size ), 6, 6 );

	// 2. Solid orange circle badge
	painter->setBrush( QColor( 245, 130, 20 ) );
	painter->setPen( Qt::NoPen );
	painter->drawEllipse( QRect( x + 5, y + 5, 22, 22 ) );

	// 3. Crisp white pause symbol "||"
	painter->setBrush( Qt::white );
	painter->drawRoundedRect( QRect( x + 11, y + 10, 3, 12 ), 1, 1 );
	painter->drawRoundedRect( QRect( x + 18, y + 10, 3, 12 ), 1, 1 );

	painter->restore();
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
