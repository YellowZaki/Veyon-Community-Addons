/*
 * ScreenRecorderEncodingWorker.cpp - asynchronous background video encoding worker
 *
 * Part of Veyon Community Addons
 */

#include "ScreenRecorderEncodingWorker.h"
#include "MediaFoundationVideoWriter.h"
#include "AviVideoWriter.h"
#include "Logger.h"

#include <QPainter>

ScreenRecorderEncodingWorker::ScreenRecorderEncodingWorker( const Options& options, QObject* parent ) :
	QThread( parent ),
	m_options( options ),
	m_actualFilePath( options.filePath )
{
}

ScreenRecorderEncodingWorker::~ScreenRecorderEncodingWorker()
{
	stopRecording();
}

bool ScreenRecorderEncodingWorker::isRecordingActive() const
{
	QMutexLocker locker( &m_mutex );
	return isRunning() && !m_stopping;
}

quint64 ScreenRecorderEncodingWorker::framesRecorded() const
{
	QMutexLocker locker( &m_mutex );
	return m_framesRecorded;
}

QString ScreenRecorderEncodingWorker::actualFilePath() const
{
	QMutexLocker locker( &m_mutex );
	return m_actualFilePath;
}

QString ScreenRecorderEncodingWorker::computerName() const
{
	return m_options.computerName;
}

void ScreenRecorderEncodingWorker::enqueueFrame( const QImage& frame )
{
	if ( frame.isNull() )
	{
		return;
	}

	QMutexLocker locker( &m_mutex );
	if ( m_stopping )
	{
		return;
	}

	if ( !m_started )
	{
		m_timer.start();
		m_started = true;
	}

	if ( m_frameQueue.size() >= MaxQueueSize )
	{
		// Drop oldest frame to prevent unbounded memory growth if disk I/O lags
		m_frameQueue.dequeue();
	}

	QueuedFrame qf;
	qf.image = frame;
	qf.timestampMs = m_timer.elapsed();
	m_frameQueue.enqueue( qf );

	m_waitCondition.wakeOne();
}

void ScreenRecorderEncodingWorker::stopRecording()
{
	{
		QMutexLocker locker( &m_mutex );
		if ( m_stopping )
		{
			return;
		}
		m_stopping = true;
		if ( m_started )
		{
			m_durationMs = m_timer.elapsed();
		}
		m_waitCondition.wakeAll();
	}

	wait();
}

QImage ScreenRecorderEncodingWorker::processFrameDimensions( const QImage& frame )
{
	if ( m_options.useFixedResolution )
	{
		const int targetW = ( m_options.resolutionWidth + 1 ) & ~1;
		const int targetH = ( m_options.resolutionHeight + 1 ) & ~1;

		if ( m_options.keepAspectRatio )
		{
			QImage canvas( targetW, targetH, QImage::Format_RGB32 );
			canvas.fill( Qt::black );

			const QImage scaled = frame.scaled( targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation );
			QPainter painter( &canvas );
			const int x = ( targetW - scaled.width() ) / 2;
			const int y = ( targetH - scaled.height() ) / 2;
			painter.drawImage( x, y, scaled );
			painter.end();
			return canvas;
		}

		return frame.scaled( targetW, targetH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
	}

	// Dynamic resolution: ensure even dimensions for H.264 / codecs
	const int evenW = ( frame.width() + 1 ) & ~1;
	const int evenH = ( frame.height() + 1 ) & ~1;

	if ( frame.width() != evenW || frame.height() != evenH )
	{
		return frame.scaled( evenW, evenH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
	}

	return frame;
}

bool ScreenRecorderEncodingWorker::initializeWriter( const QImage& firstFrame )
{
	const int width = firstFrame.width();
	const int height = firstFrame.height();

	if ( m_options.format == Format::StandardMp4 && MediaFoundationVideoWriter::isSupported() )
	{
		auto mfWriter = new MediaFoundationVideoWriter();
		if ( mfWriter->open( m_actualFilePath, width, height, m_options.fps, m_options.bitrateKbps ) )
		{
			m_writer.reset( mfWriter );
			vDebug() << "Initialized MediaFoundationVideoWriter (MP4) for" << m_options.computerName << ":" << m_actualFilePath;
			return true;
		}

		vWarning() << "MediaFoundationVideoWriter failed (" << mfWriter->errorString()
				   << "), falling back to AviVideoWriter";
		delete mfWriter;

		// Fallback to AVI
		if ( m_actualFilePath.endsWith( QStringLiteral( ".mp4" ), Qt::CaseInsensitive ) )
		{
			m_actualFilePath.chop( 4 );
			m_actualFilePath += QStringLiteral( ".avi" );
		}
	}

	auto aviWriter = new AviVideoWriter();
	if ( aviWriter->open( m_actualFilePath, width, height, m_options.fps, m_options.bitrateKbps ) )
	{
		m_writer.reset( aviWriter );
		vDebug() << "Initialized AviVideoWriter (AVI) for" << m_options.computerName << ":" << m_actualFilePath;
		return true;
	}

	vCritical() << "Failed to initialize video writer:" << aviWriter->errorString();
	const QString err = aviWriter->errorString();
	delete aviWriter;
	Q_EMIT recordingError( err );
	return false;
}

void ScreenRecorderEncodingWorker::run()
{
	vDebug() << "Starting encoding worker thread for" << m_options.computerName;

	while ( true )
	{
		QueuedFrame frame;
		{
			QMutexLocker locker( &m_mutex );
			while ( m_frameQueue.isEmpty() && !m_stopping )
			{
				m_waitCondition.wait( &m_mutex );
			}

			if ( m_frameQueue.isEmpty() && m_stopping )
			{
				break;
			}

			frame = m_frameQueue.dequeue();
		}

		const QImage processed = processFrameDimensions( frame.image );

		if ( m_writer.isNull() )
		{
			if ( !initializeWriter( processed ) )
			{
				break;
			}
			Q_EMIT recordingStarted( m_actualFilePath );
		}

		if ( m_writer && m_writer->isOpen() )
		{
			if ( m_writer->writeFrame( processed, frame.timestampMs ) )
			{
				QMutexLocker locker( &m_mutex );
				m_framesRecorded++;
			}
		}
	}

	if ( m_writer && m_writer->isOpen() )
	{
		m_writer->close();
	}

	vDebug() << "Encoding worker thread finished for" << m_options.computerName
			 << "- total frames:" << m_framesRecorded
			 << "duration:" << m_durationMs << "ms";

	Q_EMIT recordingFinished( m_actualFilePath, m_framesRecorded, m_durationMs );
}
