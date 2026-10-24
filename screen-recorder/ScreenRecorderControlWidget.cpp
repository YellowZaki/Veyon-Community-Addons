/*
 * ScreenRecorderControlWidget.cpp - recording control widget for Veyon Master
 *
 * Part of Veyon Community Addons
 */

#include "ScreenRecorderControlWidget.h"
#include "ui_ScreenRecorderControlWidget.h"

#include <QDesktopServices>
#include <QUrl>
#include <QTime>
#include <QCloseEvent>

ScreenRecorderControlWidget::ScreenRecorderControlWidget( QWidget* parent ) :
	QWidget( parent, Qt::Tool | Qt::WindowStaysOnTopHint ),
	ui( new Ui::ScreenRecorderControlWidget )
{
	ui->setupUi( this );
	setAttribute( Qt::WA_DeleteOnClose, false );

	connect( &m_tickTimer, &QTimer::timeout, this, &ScreenRecorderControlWidget::onTimerTick );
	connect( ui->stopButton, &QPushButton::clicked, this, &ScreenRecorderControlWidget::onStopButtonClicked );
	connect( ui->openFolderButton, &QPushButton::clicked, this, &ScreenRecorderControlWidget::onOpenFolderButtonClicked );
	connect( ui->closeButton, &QPushButton::clicked, this, &ScreenRecorderControlWidget::onCloseButtonClicked );
}

ScreenRecorderControlWidget::~ScreenRecorderControlWidget()
{
	delete ui;
}

void ScreenRecorderControlWidget::start( int computerCount, const QString& outputFolder )
{
	m_outputFolder = outputFolder;
	m_computerCount = computerCount;
	m_recording = true;
	m_indicatorBlink = true;

	ui->stopButton->setEnabled( true );
	ui->stopButton->setVisible( true );
	ui->recordIndicator->setVisible( true );
	ui->recordIndicator->setStyleSheet( QStringLiteral( "background-color: #e53e3e; border-radius: 8px;" ) );

	setComputerCount( computerCount );

	m_elapsedTimer.start();
	ui->durationLabel->setText( QStringLiteral( "00:00:00" ) );

	m_tickTimer.start( 1000 );
	show();
	raise();
	activateWindow();
}

void ScreenRecorderControlWidget::stop( const QString& finalDuration )
{
	m_recording = false;
	m_tickTimer.stop();

	const QString duration = finalDuration.isEmpty() ? currentDurationFormatted() : finalDuration;
	ui->durationLabel->setText( duration );
	ui->statusLabel->setText( tr( "Recording finished (duration: %1)" ).arg( duration ) );

	ui->recordIndicator->setStyleSheet( QStringLiteral( "background-color: #718096; border-radius: 8px;" ) );
	ui->stopButton->setEnabled( false );
}

void ScreenRecorderControlWidget::setComputerCount( int count )
{
	m_computerCount = count;
	if ( m_recording )
	{
		ui->statusLabel->setText( tr( "Recording %n screen(s)…", nullptr, count ) );
	}
}

void ScreenRecorderControlWidget::setOutputFolder( const QString& folder )
{
	m_outputFolder = folder;
}

bool ScreenRecorderControlWidget::isRecording() const
{
	return m_recording;
}

QString ScreenRecorderControlWidget::currentDurationFormatted() const
{
	if ( !m_elapsedTimer.isValid() )
	{
		return QStringLiteral( "00:00:00" );
	}

	const qint64 totalSecs = m_elapsedTimer.elapsed() / 1000;
	const int hours = static_cast<int>( totalSecs / 3600 );
	const int minutes = static_cast<int>( ( totalSecs % 3600 ) / 60 );
	const int seconds = static_cast<int>( totalSecs % 60 );

	return QStringLiteral( "%1:%2:%3" )
		.arg( hours, 2, 10, QLatin1Char( '0' ) )
		.arg( minutes, 2, 10, QLatin1Char( '0' ) )
		.arg( seconds, 2, 10, QLatin1Char( '0' ) );
}

void ScreenRecorderControlWidget::onTimerTick()
{
	ui->durationLabel->setText( currentDurationFormatted() );

	// Blink indicator dot
	m_indicatorBlink = !m_indicatorBlink;
	ui->recordIndicator->setStyleSheet( m_indicatorBlink
		? QStringLiteral( "background-color: #e53e3e; border-radius: 8px;" )
		: QStringLiteral( "background-color: #feb2b2; border-radius: 8px;" ) );
}

void ScreenRecorderControlWidget::onStopButtonClicked()
{
	ui->stopButton->setEnabled( false );
	ui->statusLabel->setText( tr( "Finishing recordings…" ) );
	Q_EMIT stopRecordingRequested();
}

void ScreenRecorderControlWidget::onOpenFolderButtonClicked()
{
	if ( !m_outputFolder.isEmpty() )
	{
		QDesktopServices::openUrl( QUrl::fromLocalFile( m_outputFolder ) );
	}
}

void ScreenRecorderControlWidget::onCloseButtonClicked()
{
	hide();
}

void ScreenRecorderControlWidget::closeEvent( QCloseEvent* event )
{
	hide();
	event->ignore();
}
