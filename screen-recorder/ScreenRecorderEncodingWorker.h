/*
 * ScreenRecorderEncodingWorker.h - asynchronous background video encoding worker
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QMutex>
#include <QQueue>
#include <QScopedPointer>
#include <QString>
#include <QThread>
#include <QWaitCondition>

#include "VideoWriter.h"

class ScreenRecorderEncodingWorker : public QThread
{
	Q_OBJECT
public:
	enum class Format
	{
		StandardMp4 = 0,
		MotionJpegAvi = 1
	};
	Q_ENUM( Format )

	struct Options
	{
		QString filePath;
		QString computerName;
		Format format{Format::StandardMp4};
		int fps{15};
		int bitrateKbps{2500};
		bool useFixedResolution{false};
		int resolutionWidth{1920};
		int resolutionHeight{1080};
		bool keepAspectRatio{true};
	};

	explicit ScreenRecorderEncodingWorker( const Options& options, QObject* parent = nullptr );
	~ScreenRecorderEncodingWorker() override;

	void enqueueFrame( const QImage& frame );
	void stopRecording();

	bool isRecordingActive() const;
	quint64 framesRecorded() const;
	QString actualFilePath() const;
	QString computerName() const;

Q_SIGNALS:
	void recordingStarted( const QString& filePath );
	void recordingFinished( const QString& filePath, quint64 frameCount, qint64 durationMs );
	void recordingError( const QString& errorMessage );

protected:
	void run() override;

private:
	struct QueuedFrame
	{
		QImage image;
		qint64 timestampMs{0};
	};

	bool initializeWriter( const QImage& firstFrame );
	QImage processFrameDimensions( const QImage& frame );

	const Options m_options;
	QString m_actualFilePath;

	QScopedPointer<VideoWriter> m_writer;
	QQueue<QueuedFrame> m_frameQueue;
	mutable QMutex m_mutex;
	QWaitCondition m_waitCondition;

	bool m_stopping{false};
	bool m_started{false};
	quint64 m_framesRecorded{0};
	QElapsedTimer m_timer;
	qint64 m_durationMs{0};

	static constexpr int MaxQueueSize = 30;
};
