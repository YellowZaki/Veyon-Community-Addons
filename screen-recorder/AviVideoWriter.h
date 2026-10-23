/*
 * AviVideoWriter.h - Motion-JPEG in AVI video writer implementation
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "VideoWriter.h"

#include <QFile>
#include <QVector>

class AviVideoWriter : public VideoWriter
{
public:
	AviVideoWriter();
	~AviVideoWriter() override;

	bool open( const QString& filePath, int width, int height, int fps, int bitrateKbps ) override;
	bool writeFrame( const QImage& frame, qint64 timestampMs ) override;
	void close() override;
	bool isOpen() const override;
	QString errorString() const override;

private:
	struct AviIndexEntry
	{
		quint32 ckid;
		quint32 flags;
		quint32 chunkOffset;
		quint32 chunkLength;
	};

	void writeAviHeaders();
	void updateAviHeaders();

	QFile m_file;
	QString m_errorString;
	int m_width{0};
	int m_height{0};
	int m_fps{15};
	int m_quality{85};
	quint32 m_frameCount{0};

	qint64 m_riffSizeOffset{0};
	qint64 m_avihFramesOffset{0};
	qint64 m_strhLengthOffset{0};
	qint64 m_moviSizeOffset{0};
	qint64 m_moviDataOffset{0};

	QVector<AviIndexEntry> m_index;
};
