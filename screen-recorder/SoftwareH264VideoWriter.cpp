/*
 * SoftwareH264VideoWriter.cpp - Pure software CPU H.264 / MP4 video writer
 *
 * Part of Veyon Community Addons
 */

#include "SoftwareH264VideoWriter.h"
#include "Logger.h"

#include <QDir>
#include <QFileInfo>
#include <cstdlib>

#include "3rdparty/minih264e.h"
#include "3rdparty/minimp4.h"

static int mp4WriteCallback( int64_t offset, const void* buffer, size_t size, void* token )
{
	FILE* f = static_cast<FILE*>( token );
#if defined(_WIN32)
	if ( _fseeki64( f, offset, SEEK_SET ) != 0 )
	{
		return -1;
	}
#else
	if ( fseeko( f, offset, SEEK_SET ) != 0 )
	{
		return -1;
	}
#endif
	return ( fwrite( buffer, 1, size, f ) == size ) ? 0 : -1;
}

SoftwareH264VideoWriter::SoftwareH264VideoWriter() = default;

SoftwareH264VideoWriter::~SoftwareH264VideoWriter()
{
	close();
}

bool SoftwareH264VideoWriter::isSupported()
{
	return true; // Always supported on all platforms & architectures
}

bool SoftwareH264VideoWriter::isOpen() const
{
	return m_isOpen && m_file != nullptr && m_mux != nullptr && m_enc != nullptr;
}

QString SoftwareH264VideoWriter::errorString() const
{
	return m_errorString;
}

void SoftwareH264VideoWriter::freeResources()
{
	if ( m_mp4Writer )
	{
		auto mp4Wr = static_cast<mp4_h26x_writer_t*>( m_mp4Writer );
		mp4_h26x_write_close( mp4Wr );
		delete mp4Wr;
		m_mp4Writer = nullptr;
	}

	if ( m_mux )
	{
		MP4E_close( m_mux );
		m_mux = nullptr;
	}

	if ( m_file )
	{
		fclose( m_file );
		m_file = nullptr;
	}

	if ( m_enc )
	{
		free( m_enc );
		m_enc = nullptr;
	}

	if ( m_scratch )
	{
		free( m_scratch );
		m_scratch = nullptr;
	}

	m_yuvBuffer.clear();
	m_isOpen = false;
}

void SoftwareH264VideoWriter::close()
{
	freeResources();
}

void SoftwareH264VideoWriter::convertRgb32ToYuv420(
	const QImage& img,
	unsigned char* yPlane,
	unsigned char* uPlane,
	unsigned char* vPlane,
	int width,
	int height )
{
	const int halfW = width / 2;

	for ( int y = 0; y < height; y += 2 )
	{
		unsigned char* yRow0 = yPlane + ( y * width );
		unsigned char* yRow1 = yPlane + ( ( y + 1 ) * width );
		unsigned char* uRow = uPlane + ( ( y / 2 ) * halfW );
		unsigned char* vRow = vPlane + ( ( y / 2 ) * halfW );

		const QRgb* srcRow0 = reinterpret_cast<const QRgb*>( img.constScanLine( y ) );
		const QRgb* srcRow1 = reinterpret_cast<const QRgb*>( img.constScanLine( y + 1 ) );

		for ( int x = 0; x < width; x += 2 )
		{
			const QRgb p00 = srcRow0[x];
			const QRgb p01 = srcRow0[x + 1];
			const QRgb p10 = srcRow1[x];
			const QRgb p11 = srcRow1[x + 1];

			const int r00 = qRed( p00 ), g00 = qGreen( p00 ), b00 = qBlue( p00 );
			const int r01 = qRed( p01 ), g01 = qGreen( p01 ), b01 = qBlue( p01 );
			const int r10 = qRed( p10 ), g10 = qGreen( p10 ), b10 = qBlue( p10 );
			const int r11 = qRed( p11 ), g11 = qGreen( p11 ), b11 = qBlue( p11 );

			// ITU-R BT.601 standard RGB to Y
			yRow0[x]     = static_cast<unsigned char>( qBound( 0, ( (  66 * r00 + 129 * g00 +  25 * b00 + 128 ) >> 8 ) + 16, 255 ) );
			yRow0[x + 1] = static_cast<unsigned char>( qBound( 0, ( (  66 * r01 + 129 * g01 +  25 * b01 + 128 ) >> 8 ) + 16, 255 ) );
			yRow1[x]     = static_cast<unsigned char>( qBound( 0, ( (  66 * r10 + 129 * g10 +  25 * b10 + 128 ) >> 8 ) + 16, 255 ) );
			yRow1[x + 1] = static_cast<unsigned char>( qBound( 0, ( (  66 * r11 + 129 * g11 +  25 * b11 + 128 ) >> 8 ) + 16, 255 ) );

			// Average 2x2 chroma subsample block
			const int avgR = ( r00 + r01 + r10 + r11 + 2 ) >> 2;
			const int avgG = ( g00 + g01 + g10 + g11 + 2 ) >> 2;
			const int avgB = ( b00 + b01 + b10 + b11 + 2 ) >> 2;

			// ITU-R BT.601 standard RGB to U and V
			const int u = ( ( -38 * avgR - 74 * avgG + 112 * avgB + 128 ) >> 8 ) + 128;
			const int v = ( ( 112 * avgR - 94 * avgG -  18 * avgB + 128 ) >> 8 ) + 128;

			const int uvIdx = x / 2;
			uRow[uvIdx] = static_cast<unsigned char>( qBound( 0, u, 255 ) );
			vRow[uvIdx] = static_cast<unsigned char>( qBound( 0, v, 255 ) );
		}
	}
}

bool SoftwareH264VideoWriter::open( const QString& filePath, int width, int height, int fps, int quality )
{
	close();

	if ( width <= 0 || height <= 0 || fps <= 0 )
	{
		m_errorString = QStringLiteral( "Invalid dimensions or frame rate" );
		return false;
	}

	// minih264e requires width and height to be multiples of 16
	m_width = ( width + 15 ) & ~15;
	m_height = ( height + 15 ) & ~15;
	m_fps = fps;
	m_quality = qBound( 1, quality, 100 );
	m_filePath = filePath;
	m_lastTimestampMs = -1;

	QFileInfo fi( filePath );
	QDir().mkpath( fi.absolutePath() );
	const QString nativePath = QDir::toNativeSeparators( filePath );

#if defined(_WIN32)
	m_file = _wfopen( reinterpret_cast<const wchar_t*>( nativePath.utf16() ), L"w+b" );
#else
	m_file = fopen( nativePath.toUtf8().constData(), "w+b" );
#endif

	if ( !m_file )
	{
		m_errorString = QStringLiteral( "Failed to create output file: %1" ).arg( nativePath );
		return false;
	}

	m_mux = MP4E_open( 0, 0, m_file, mp4WriteCallback );
	if ( !m_mux )
	{
		m_errorString = QStringLiteral( "Failed to initialize MP4 multiplexer" );
		close();
		return false;
	}

	auto mp4Wr = new mp4_h26x_writer_t();
	memset( mp4Wr, 0, sizeof( mp4_h26x_writer_t ) );
	if ( mp4_h26x_write_init( mp4Wr, m_mux, m_width, m_height, 0 /* H.264 */ ) != MP4E_STATUS_OK )
	{
		delete mp4Wr;
		m_errorString = QStringLiteral( "Failed to initialize MP4 H.264 writer" );
		close();
		return false;
	}
	m_mp4Writer = mp4Wr;

	H264E_create_param_t param;
	memset( &param, 0, sizeof( param ) );
	param.width = m_width;
	param.height = m_height;
	param.gop = m_fps * 2; // Keyframe every 2 seconds
	param.enableNEON = 0;

	int sizeof_persist = 0;
	int sizeof_scratch = 0;
	int err = H264E_sizeof( &param, &sizeof_persist, &sizeof_scratch );
	if ( err != H264E_STATUS_SUCCESS )
	{
		m_errorString = QStringLiteral( "H264E_sizeof failed (%1)" ).arg( err );
		close();
		return false;
	}

	m_enc = static_cast<H264E_persist_t*>( malloc( sizeof_persist ) );
	m_scratch = static_cast<H264E_scratch_t*>( malloc( sizeof_scratch ) );
	if ( !m_enc || !m_scratch )
	{
		m_errorString = QStringLiteral( "Failed to allocate memory for software H.264 encoder" );
		close();
		return false;
	}

	err = H264E_init( m_enc, &param );
	if ( err != H264E_STATUS_SUCCESS )
	{
		m_errorString = QStringLiteral( "H264E_init failed (%1)" ).arg( err );
		close();
		return false;
	}

	const int yuvSize = ( m_width * m_height * 3 ) / 2;
	m_yuvBuffer.resize( yuvSize );

	m_isOpen = true;
	return true;
}

bool SoftwareH264VideoWriter::writeFrame( const QImage& frame, qint64 timestampMs )
{
	if ( !m_isOpen || !m_enc || !m_scratch || !m_mp4Writer )
	{
		m_errorString = QStringLiteral( "Writer is not open" );
		return false;
	}

	QImage img = frame;
	if ( img.size() != QSize( m_width, m_height ) )
	{
		img = img.scaled( m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
	}
	if ( img.format() != QImage::Format_RGB32 && img.format() != QImage::Format_ARGB32 )
	{
		img = img.convertToFormat( QImage::Format_RGB32 );
	}

	unsigned char* yPlane = reinterpret_cast<unsigned char*>( m_yuvBuffer.data() );
	unsigned char* uPlane = yPlane + ( m_width * m_height );
	unsigned char* vPlane = uPlane + ( ( m_width * m_height ) / 4 );

	convertRgb32ToYuv420( img, yPlane, uPlane, vPlane, m_width, m_height );

	H264E_io_yuv_t yuv;
	yuv.yuv[0] = yPlane;
	yuv.yuv[1] = uPlane;
	yuv.yuv[2] = vPlane;
	yuv.stride[0] = m_width;
	yuv.stride[1] = m_width / 2;
	yuv.stride[2] = m_width / 2;

	H264E_run_param_t run;
	memset( &run, 0, sizeof( run ) );
	run.frame_type = 0; // Auto I/P based on GOP
	run.encode_speed = 9; // Fast preset
	run.qp_min = 10;
	run.qp_max = qBound( 12, 51 - ( m_quality * 36 / 100 ), 51 );

	unsigned char* codedData = nullptr;
	int sizeofCodedData = 0;

	int err = H264E_encode( m_enc, m_scratch, &run, &yuv, &codedData, &sizeofCodedData );
	if ( err != H264E_STATUS_SUCCESS )
	{
		m_errorString = QStringLiteral( "H264E_encode failed (%1)" ).arg( err );
		return false;
	}

	if ( sizeofCodedData > 0 && codedData )
	{
		unsigned int durationTs = 90000 / static_cast<unsigned int>( m_fps );
		if ( m_lastTimestampMs >= 0 && timestampMs > m_lastTimestampMs )
		{
			const qint64 deltaMs = timestampMs - m_lastTimestampMs;
			durationTs = static_cast<unsigned int>( deltaMs * 90 );
		}
		m_lastTimestampMs = timestampMs;

		auto mp4Wr = static_cast<mp4_h26x_writer_t*>( m_mp4Writer );
		int status = mp4_h26x_write_nal( mp4Wr, codedData, sizeofCodedData, durationTs );
		if ( status != MP4E_STATUS_OK )
		{
			m_errorString = QStringLiteral( "mp4_h26x_write_nal failed (%1)" ).arg( status );
			return false;
		}
	}

	return true;
}
