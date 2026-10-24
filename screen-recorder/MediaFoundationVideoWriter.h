/*
 * MediaFoundationVideoWriter.h - Windows Media Foundation H.264 / MP4 writer
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "VideoWriter.h"

struct IMFSinkWriter;

class MediaFoundationVideoWriter : public VideoWriter
{
public:
	enum class Codec
	{
		H264,
		H265
	};

	explicit MediaFoundationVideoWriter( Codec codec = Codec::H264 );
	~MediaFoundationVideoWriter() override;

	bool open( const QString& filePath, int width, int height, int fps, int bitrateKbps ) override;
	bool writeFrame( const QImage& frame, qint64 timestampMs ) override;
	void close() override;
	bool isOpen() const override;
	QString errorString() const override;

	static bool isSupported();

private:
	Codec m_codec{Codec::H264};
	IMFSinkWriter* m_sinkWriter{nullptr};
	unsigned long m_streamIndex{0};
	bool m_initialized{false};
	bool m_mfStarted{false};
	bool m_comInitialized{false};
	int m_width{0};
	int m_height{0};
	int m_fps{15};
	QString m_errorString;
};
