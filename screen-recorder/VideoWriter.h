/*
 * VideoWriter.h - abstract interface for video writers
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QImage>
#include <QString>

class VideoWriter
{
public:
	virtual ~VideoWriter() = default;

	virtual bool open( const QString& filePath, int width, int height, int fps, int quality ) = 0;
	virtual bool writeFrame( const QImage& frame, qint64 timestampMs ) = 0;
	virtual void close() = 0;
	virtual bool isOpen() const = 0;
	virtual QString errorString() const = 0;
};
