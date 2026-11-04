/*
 * SoftwareH264VideoWriter.h - Pure software CPU H.264 / MP4 video writer
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "VideoWriter.h"
#include <QString>
#include <QImage>
#include <QByteArray>
#include <cstdio>

struct H264E_persist_tag;
struct H264E_scratch_tag;
struct MP4E_mux_tag;

class SoftwareH264VideoWriter : public VideoWriter
{
public:
	SoftwareH264VideoWriter();
	~SoftwareH264VideoWriter() override;

	static bool isSupported();

	bool open( const QString& filePath, int width, int height, int fps, int quality ) override;
	bool writeFrame( const QImage& frame, qint64 timestampMs ) override;
	void close() override;
	bool isOpen() const override;
	QString errorString() const override;

private:
	void freeResources();
	static void convertRgb32ToYuv420( const QImage& img, unsigned char* yPlane, unsigned char* uPlane, unsigned char* vPlane, int width, int height );

	bool m_isOpen{false};
	QString m_errorString;
	QString m_filePath;

	int m_width{0};
	int m_height{0};
	int m_fps{15};
	int m_quality{80};
	qint64 m_lastTimestampMs{-1};

	FILE* m_file{nullptr};
	MP4E_mux_tag* m_mux{nullptr};
	void* m_mp4Writer{nullptr};

	H264E_persist_tag* m_enc{nullptr};
	H264E_scratch_tag* m_scratch{nullptr};

	QByteArray m_yuvBuffer;
};
