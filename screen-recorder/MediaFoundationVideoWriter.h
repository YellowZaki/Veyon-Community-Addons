/*
 * MediaFoundationVideoWriter.h - Windows Media Foundation H.264 / MP4 writer
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include "VideoWriter.h"

struct IMFSinkWriter;
typedef struct _GUID GUID;

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

	bool open( const QString& filePath, int width, int height, int fps, int quality ) override;
	bool writeFrame( const QImage& frame, qint64 timestampMs ) override;
	void close() override;
	bool isOpen() const override;
	QString errorString() const override;

	static bool isSupported();

private:
	enum class InputPixelFormat
	{
		NV12,
		YUY2,
		RGB32
	};

	bool createPipeline( const QString& nativePath, Codec codec, bool enableHardwareTransforms, int width, int height );
	bool trySetInputType( IMFSinkWriter* pWriter, unsigned long streamIndex, const GUID& subtype, int width, int height, int fps, bool fullAttributes, long& outHr );
	void cleanupSinkWriter( const QString& nativePath );
	static void convertRgb32ToNv12( const QImage& img, unsigned char* pDst, int width, int height );
	static void convertRgb32ToYuy2( const QImage& img, unsigned char* pDst, int width, int height );

	Codec m_codec{Codec::H264};
	InputPixelFormat m_inputFormat{InputPixelFormat::NV12};
	IMFSinkWriter* m_sinkWriter{nullptr};
	unsigned long m_streamIndex{0};
	bool m_initialized{false};
	bool m_mfStarted{false};
	bool m_comInitialized{false};
	int m_width{0};
	int m_height{0};
	int m_fps{15};
	int m_quality{70};
	QString m_errorString;
};
