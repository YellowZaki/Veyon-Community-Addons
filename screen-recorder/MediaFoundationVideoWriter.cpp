/*
 * MediaFoundationVideoWriter.cpp - Windows Media Foundation H.264 / MP4 writer
 *
 * Part of Veyon Community Addons
 */

#include "MediaFoundationVideoWriter.h"
#include "Logger.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>

#ifdef Q_OS_WIN

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// Static GUID definitions to avoid linking external mfuuid.lib/libmfuuid.a
static const GUID GUID_MF_MT_MAJOR_TYPE =
	{ 0x48eba18e, 0xf8c9, 0x4687, { 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f } };

static const GUID GUID_MF_MT_SUBTYPE =
	{ 0xf7e34c9a, 0x42e8, 0x4714, { 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5 } };

static const GUID GUID_MFMediaType_Video =
	{ 0x73646976, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static const GUID GUID_MFVideoFormat_H264 =
	{ 0x34363248, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static const GUID GUID_MFVideoFormat_HEVC =
	{ 0x43564548, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static const GUID GUID_MFVideoFormat_RGB32 =
	{ 0x00000016, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static const GUID GUID_MFVideoFormat_NV12 =
	{ 0x3231564e, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static const GUID GUID_MFVideoFormat_YUY2 =
	{ 0x32595559, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static const GUID GUID_MF_MT_AVG_BITRATE =
	{ 0x20332624, 0xfb0d, 0x4d9e, { 0xbd, 0x0d, 0xcb, 0xf6, 0x78, 0x6c, 0x10, 0x2e } };

static const GUID GUID_MF_MT_FRAME_SIZE =
	{ 0x1652c33d, 0xd6b2, 0x4012, { 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d } };

static const GUID GUID_MF_MT_FRAME_RATE =
	{ 0xc459a2e8, 0x3d2c, 0x4e44, { 0xb1, 0x32, 0xfe, 0xe5, 0x15, 0x6c, 0x7b, 0xb0 } };

static const GUID GUID_MF_MT_PIXEL_ASPECT_RATIO =
	{ 0xc6376a1e, 0x8d0a, 0x4027, { 0xbe, 0x45, 0x6d, 0x9a, 0x0a, 0xd3, 0x9b, 0xb6 } };

static const GUID GUID_MF_MT_INTERLACE_MODE =
	{ 0xe2724bb8, 0xe676, 0x4806, { 0xb4, 0xb2, 0xa8, 0xd6, 0xef, 0x54, 0xd2, 0xf6 } };

static const GUID GUID_MF_MT_ALL_SAMPLES_INDEPENDENT =
	{ 0xc9173739, 0x5e56, 0x461c, { 0xb7, 0x13, 0x46, 0xfb, 0x99, 0x5c, 0xb9, 0x5f } };

static const GUID GUID_MF_MT_DEFAULT_STRIDE =
	{ 0x644b4e48, 0x1e02, 0x4516, { 0xb0, 0xeb, 0xc0, 0x1c, 0xa9, 0xd4, 0x9a, 0xc6 } };

static const GUID GUID_MF_MT_FIXED_SIZE_SAMPLES =
	{ 0xb8ebefaf, 0xb718, 0x4e04, { 0xb0, 0xa9, 0x11, 0x67, 0x75, 0xe3, 0x32, 0x1b } };

static const GUID GUID_MF_MT_SAMPLE_SIZE =
	{ 0xdad3ab78, 0x1990, 0x408b, { 0xbc, 0xe2, 0xeb, 0xa6, 0x73, 0xda, 0xcc, 0x10 } };

static const GUID GUID_MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS =
	{ 0xa634a91c, 0x822b, 0x41b9, { 0xa4, 0x94, 0x4d, 0xe4, 0x64, 0x36, 0x12, 0xb0 } };

static const GUID GUID_MF_SINK_WRITER_DISABLE_THROTTLING =
	{ 0x08b845d8, 0x2b74, 0x4afe, { 0x9d, 0x53, 0xbe, 0x16, 0xd2, 0xd5, 0xae, 0x4f } };

// Dynamic loader for Media Foundation functions
class MediaFoundationApi
{
public:
	typedef HRESULT (WINAPI *MFStartup_t)(ULONG, DWORD);
	typedef HRESULT (WINAPI *MFShutdown_t)();
	typedef HRESULT (WINAPI *MFCreateAttributes_t)(IMFAttributes**, UINT32);
	typedef HRESULT (WINAPI *MFCreateMediaType_t)(IMFMediaType**);
	typedef HRESULT (WINAPI *MFCreateMemoryBuffer_t)(DWORD, IMFMediaBuffer**);
	typedef HRESULT (WINAPI *MFCreateSample_t)(IMFSample**);
	typedef HRESULT (WINAPI *MFCreateSinkWriterFromURL_t)(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**);
	typedef HRESULT (WINAPI *MFTRegisterLocalByCLSID_t)(REFCLSID, REFGUID, LPCWSTR, UINT32, UINT32, const void*, UINT32, const void*);

	static MediaFoundationApi& instance()
	{
		static MediaFoundationApi s_instance;
		return s_instance;
	}

	bool isValid() const { return m_valid; }

	MFStartup_t fnStartup{nullptr};
	MFShutdown_t fnShutdown{nullptr};
	MFCreateAttributes_t fnCreateAttributes{nullptr};
	MFCreateMediaType_t fnCreateMediaType{nullptr};
	MFCreateMemoryBuffer_t fnCreateMemoryBuffer{nullptr};
	MFCreateSample_t fnCreateSample{nullptr};
	MFCreateSinkWriterFromURL_t fnCreateSinkWriterFromURL{nullptr};
	MFTRegisterLocalByCLSID_t fnMFTRegisterLocalByCLSID{nullptr};

private:
	MediaFoundationApi()
	{
		m_hMfplat = LoadLibraryW( L"mfplat.dll" );
		m_hMfReadWrite = LoadLibraryW( L"mfreadwrite.dll" );

		if ( m_hMfplat && m_hMfReadWrite )
		{
			fnStartup = reinterpret_cast<MFStartup_t>( GetProcAddress( m_hMfplat, "MFStartup" ) );
			fnShutdown = reinterpret_cast<MFShutdown_t>( GetProcAddress( m_hMfplat, "MFShutdown" ) );
			fnCreateAttributes = reinterpret_cast<MFCreateAttributes_t>( GetProcAddress( m_hMfplat, "MFCreateAttributes" ) );
			fnCreateMediaType = reinterpret_cast<MFCreateMediaType_t>( GetProcAddress( m_hMfplat, "MFCreateMediaType" ) );
			fnCreateMemoryBuffer = reinterpret_cast<MFCreateMemoryBuffer_t>( GetProcAddress( m_hMfplat, "MFCreateMemoryBuffer" ) );
			fnCreateSample = reinterpret_cast<MFCreateSample_t>( GetProcAddress( m_hMfplat, "MFCreateSample" ) );
			fnCreateSinkWriterFromURL = reinterpret_cast<MFCreateSinkWriterFromURL_t>( GetProcAddress( m_hMfReadWrite, "MFCreateSinkWriterFromURL" ) );
			fnMFTRegisterLocalByCLSID = reinterpret_cast<MFTRegisterLocalByCLSID_t>( GetProcAddress( m_hMfplat, "MFTRegisterLocalByCLSID" ) );

			m_valid = fnStartup && fnShutdown && fnCreateAttributes && fnCreateMediaType &&
					  fnCreateMemoryBuffer && fnCreateSample && fnCreateSinkWriterFromURL;
		}
	}

	~MediaFoundationApi()
	{
		if ( m_hMfReadWrite )
		{
			FreeLibrary( m_hMfReadWrite );
		}
		if ( m_hMfplat )
		{
			FreeLibrary( m_hMfplat );
		}
	}

	HMODULE m_hMfplat{nullptr};
	HMODULE m_hMfReadWrite{nullptr};
	bool m_valid{false};
};

static inline HRESULT setAttrSize( IMFMediaType* pType, const GUID& guid, UINT32 unWidth, UINT32 unHeight )
{
	return pType->SetUINT64( guid, ( static_cast<UINT64>( unWidth ) << 32 ) | unHeight );
}

static inline HRESULT setAttrRatio( IMFMediaType* pType, const GUID& guid, UINT32 unNumerator, UINT32 unDenominator )
{
	return pType->SetUINT64( guid, ( static_cast<UINT64>( unNumerator ) << 32 ) | unDenominator );
}

#endif // Q_OS_WIN

MediaFoundationVideoWriter::MediaFoundationVideoWriter( Codec codec ) :
	m_codec( codec )
{
}

MediaFoundationVideoWriter::~MediaFoundationVideoWriter()
{
	close();
}

bool MediaFoundationVideoWriter::isSupported()
{
#ifdef Q_OS_WIN
	return MediaFoundationApi::instance().isValid();
#else
	return false;
#endif
}

bool MediaFoundationVideoWriter::isOpen() const
{
	return m_initialized && m_sinkWriter != nullptr;
}

QString MediaFoundationVideoWriter::errorString() const
{
	return m_errorString;
}

void MediaFoundationVideoWriter::convertRgb32ToNv12( const QImage& img, unsigned char* pDst, int width, int height )
{
	unsigned char* yPlane = pDst;
	unsigned char* uvPlane = pDst + ( width * height );

	for ( int y = 0; y < height; y += 2 )
	{
		unsigned char* yRow0 = yPlane + ( y * width );
		unsigned char* yRow1 = yPlane + ( ( y + 1 ) * width );
		unsigned char* uvRow = uvPlane + ( ( y / 2 ) * width );

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

			// ITU-R BT.601 standard RGB to Y conversion
			yRow0[x]     = static_cast<unsigned char>( qBound( 0, ( (  66 * r00 + 129 * g00 +  25 * b00 + 128 ) >> 8 ) + 16, 255 ) );
			yRow0[x + 1] = static_cast<unsigned char>( qBound( 0, ( (  66 * r01 + 129 * g01 +  25 * b01 + 128 ) >> 8 ) + 16, 255 ) );
			yRow1[x]     = static_cast<unsigned char>( qBound( 0, ( (  66 * r10 + 129 * g10 +  25 * b10 + 128 ) >> 8 ) + 16, 255 ) );
			yRow1[x + 1] = static_cast<unsigned char>( qBound( 0, ( (  66 * r11 + 129 * g11 +  25 * b11 + 128 ) >> 8 ) + 16, 255 ) );

			// Average RGB in 2x2 chroma subsample block
			const int avgR = ( r00 + r01 + r10 + r11 + 2 ) >> 2;
			const int avgG = ( g00 + g01 + g10 + g11 + 2 ) >> 2;
			const int avgB = ( b00 + b01 + b10 + b11 + 2 ) >> 2;

			// ITU-R BT.601 standard RGB to UV conversion
			const int u = ( ( -38 * avgR - 74 * avgG + 112 * avgB + 128 ) >> 8 ) + 128;
			const int v = ( ( 112 * avgR - 94 * avgG -  18 * avgB + 128 ) >> 8 ) + 128;

			uvRow[x]     = static_cast<unsigned char>( qBound( 0, u, 255 ) );
			uvRow[x + 1] = static_cast<unsigned char>( qBound( 0, v, 255 ) );
		}
	}
}

void MediaFoundationVideoWriter::convertRgb32ToYuy2( const QImage& img, unsigned char* pDst, int width, int height )
{
	for ( int y = 0; y < height; ++y )
	{
		const QRgb* srcRow = reinterpret_cast<const QRgb*>( img.constScanLine( y ) );
		unsigned char* dstRow = pDst + ( y * width * 2 );

		for ( int x = 0; x < width; x += 2 )
		{
			const QRgb p0 = srcRow[x];
			const QRgb p1 = srcRow[x + 1];

			const int r0 = qRed( p0 ), g0 = qGreen( p0 ), b0 = qBlue( p0 );
			const int r1 = qRed( p1 ), g1 = qGreen( p1 ), b1 = qBlue( p1 );

			// BT.601 Y
			const int y0 = ( (  66 * r0 + 129 * g0 +  25 * b0 + 128 ) >> 8 ) + 16;
			const int y1 = ( (  66 * r1 + 129 * g1 +  25 * b1 + 128 ) >> 8 ) + 16;

			// Average RGB in 2x1 block
			const int avgR = ( r0 + r1 + 1 ) >> 1;
			const int avgG = ( g0 + g1 + 1 ) >> 1;
			const int avgB = ( b0 + b1 + 1 ) >> 1;

			// BT.601 UV
			const int u = ( ( -38 * avgR - 74 * avgG + 112 * avgB + 128 ) >> 8 ) + 128;
			const int v = ( ( 112 * avgR - 94 * avgG -  18 * avgB + 128 ) >> 8 ) + 128;

			const int dstIdx = x * 2;
			dstRow[dstIdx + 0] = static_cast<unsigned char>( qBound( 0, y0, 255 ) );
			dstRow[dstIdx + 1] = static_cast<unsigned char>( qBound( 0, u, 255 ) );
			dstRow[dstIdx + 2] = static_cast<unsigned char>( qBound( 0, y1, 255 ) );
			dstRow[dstIdx + 3] = static_cast<unsigned char>( qBound( 0, v, 255 ) );
		}
	}
}

void MediaFoundationVideoWriter::cleanupSinkWriter( const QString& nativePath )
{
#ifdef Q_OS_WIN
	if ( m_sinkWriter )
	{
		m_sinkWriter->Release();
		m_sinkWriter = nullptr;
	}
	for ( int i = 0; i < 10 && QFile::exists( nativePath ); ++i )
	{
		if ( QFile::remove( nativePath ) )
		{
			break;
		}
		Sleep( 25 );
	}
#else
	Q_UNUSED( nativePath )
#endif
}

bool MediaFoundationVideoWriter::trySetInputType(
	IMFSinkWriter* pWriter,
	unsigned long streamIndex,
	const GUID& subtype,
	int width,
	int height,
	int fps,
	bool fullAttributes,
	long& outHr )
{
#ifdef Q_OS_WIN
	auto& mf = MediaFoundationApi::instance();
	IMFMediaType* pMediaTypeIn = nullptr;
	HRESULT hr = mf.fnCreateMediaType( &pMediaTypeIn );
	if ( FAILED( hr ) || !pMediaTypeIn )
	{
		outHr = hr;
		return false;
	}

	pMediaTypeIn->SetGUID( GUID_MF_MT_MAJOR_TYPE, GUID_MFMediaType_Video );
	pMediaTypeIn->SetGUID( GUID_MF_MT_SUBTYPE, subtype );
	pMediaTypeIn->SetUINT32( GUID_MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	setAttrSize( pMediaTypeIn, GUID_MF_MT_FRAME_SIZE, static_cast<UINT32>( width ), static_cast<UINT32>( height ) );
	setAttrRatio( pMediaTypeIn, GUID_MF_MT_FRAME_RATE, static_cast<UINT32>( fps ), 1 );
	setAttrRatio( pMediaTypeIn, GUID_MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	if ( fullAttributes )
	{
		pMediaTypeIn->SetUINT32( GUID_MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE );
		pMediaTypeIn->SetUINT32( GUID_MF_MT_FIXED_SIZE_SAMPLES, TRUE );

		UINT32 stride = 0;
		UINT32 sampleSize = 0;
		if ( subtype == GUID_MFVideoFormat_NV12 )
		{
			stride = static_cast<UINT32>( width );
			sampleSize = static_cast<UINT32>( ( width * height * 3 ) / 2 );
		}
		else if ( subtype == GUID_MFVideoFormat_YUY2 )
		{
			stride = static_cast<UINT32>( width * 2 );
			sampleSize = static_cast<UINT32>( width * height * 2 );
		}
		else if ( subtype == GUID_MFVideoFormat_RGB32 )
		{
			stride = static_cast<UINT32>( width * 4 );
			sampleSize = static_cast<UINT32>( width * height * 4 );
		}

		if ( stride > 0 )
		{
			pMediaTypeIn->SetUINT32( GUID_MF_MT_DEFAULT_STRIDE, stride );
		}
		if ( sampleSize > 0 )
		{
			pMediaTypeIn->SetUINT32( GUID_MF_MT_SAMPLE_SIZE, sampleSize );
		}
	}

	hr = pWriter->SetInputMediaType( streamIndex, pMediaTypeIn, NULL );
	pMediaTypeIn->Release();
	outHr = hr;
	return SUCCEEDED( hr );
#else
	Q_UNUSED( pWriter )
	Q_UNUSED( streamIndex )
	Q_UNUSED( subtype )
	Q_UNUSED( width )
	Q_UNUSED( height )
	Q_UNUSED( fps )
	Q_UNUSED( fullAttributes )
	outHr = E_FAIL;
	return false;
#endif
}

bool MediaFoundationVideoWriter::createPipeline( const QString& nativePath, Codec codec, bool enableHardwareTransforms, int width, int height )
{
#ifdef Q_OS_WIN
	auto& mf = MediaFoundationApi::instance();

	IMFAttributes* pAttributes = nullptr;
	HRESULT hr = mf.fnCreateAttributes( &pAttributes, 2 );
	if ( SUCCEEDED( hr ) && pAttributes )
	{
		pAttributes->SetUINT32( GUID_MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, enableHardwareTransforms ? TRUE : FALSE );
		pAttributes->SetUINT32( GUID_MF_SINK_WRITER_DISABLE_THROTTLING, TRUE );
	}

	hr = mf.fnCreateSinkWriterFromURL( reinterpret_cast<LPCWSTR>( nativePath.utf16() ),
									   NULL,
									   pAttributes,
									   &m_sinkWriter );
	if ( pAttributes )
	{
		pAttributes->Release();
		pAttributes = nullptr;
	}

	if ( FAILED( hr ) || !m_sinkWriter )
	{
		m_errorString = QStringLiteral( "MFCreateSinkWriterFromURL failed (0x%1)" )
							.arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		return false;
	}

	// Output MediaType: H.264 or HEVC video
	IMFMediaType* pMediaTypeOut = nullptr;
	hr = mf.fnCreateMediaType( &pMediaTypeOut );
	if ( FAILED( hr ) || !pMediaTypeOut )
	{
		m_errorString = QStringLiteral( "MFCreateMediaType for output failed" );
		m_sinkWriter->Release();
		m_sinkWriter = nullptr;
		return false;
	}

	const GUID& targetSubtype = ( codec == Codec::H265 )
		? GUID_MFVideoFormat_HEVC
		: GUID_MFVideoFormat_H264;

	pMediaTypeOut->SetGUID( GUID_MF_MT_MAJOR_TYPE, GUID_MFMediaType_Video );
	pMediaTypeOut->SetGUID( GUID_MF_MT_SUBTYPE, targetSubtype );

	const UINT32 nominalBitrate = static_cast<UINT32>( m_quality * 50000 );
	pMediaTypeOut->SetUINT32( GUID_MF_MT_AVG_BITRATE, nominalBitrate );
	pMediaTypeOut->SetUINT32( GUID_MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	setAttrSize( pMediaTypeOut, GUID_MF_MT_FRAME_SIZE, static_cast<UINT32>( width ), static_cast<UINT32>( height ) );
	setAttrRatio( pMediaTypeOut, GUID_MF_MT_FRAME_RATE, static_cast<UINT32>( m_fps ), 1 );
	setAttrRatio( pMediaTypeOut, GUID_MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	hr = m_sinkWriter->AddStream( pMediaTypeOut, &m_streamIndex );
	pMediaTypeOut->Release();
	if ( FAILED( hr ) )
	{
		const QString codecName = ( codec == Codec::H265 ) ? QStringLiteral( "HEVC/H.265" ) : QStringLiteral( "H.264" );
		m_errorString = QStringLiteral( "AddStream (%1) failed (0x%2) for %3x%4" )
							.arg( codecName )
							.arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) )
							.arg( width )
							.arg( height );
		m_sinkWriter->Release();
		m_sinkWriter = nullptr;
		return false;
	}

	// Try input formats in priority order:
	// 1. NV12 (full attributes) - standard hardware/software input
	// 2. NV12 (minimal attributes)
	// 3. YUY2 (full attributes) - widely compatible 4:2:2 input
	// 4. YUY2 (minimal attributes)
	// 5. RGB32 (full attributes) - direct RGB32 input via system color converter
	// 6. RGB32 (minimal attributes)
	struct InputCandidate
	{
		GUID subtype;
		InputPixelFormat format;
		bool fullAttributes;
		const char* name;
	};

	const InputCandidate candidates[] = {
		{ GUID_MFVideoFormat_NV12,  InputPixelFormat::NV12,  true,  "NV12 (full)" },
		{ GUID_MFVideoFormat_NV12,  InputPixelFormat::NV12,  false, "NV12 (minimal)" },
		{ GUID_MFVideoFormat_YUY2,  InputPixelFormat::YUY2,  true,  "YUY2 (full)" },
		{ GUID_MFVideoFormat_YUY2,  InputPixelFormat::YUY2,  false, "YUY2 (minimal)" },
		{ GUID_MFVideoFormat_RGB32, InputPixelFormat::RGB32, true,  "RGB32 (full)" },
		{ GUID_MFVideoFormat_RGB32, InputPixelFormat::RGB32, false, "RGB32 (minimal)" }
	};

	bool inputConfigured = false;
	long lastInputHr = S_OK;

	for ( const auto& cand : candidates )
	{
		long candHr = S_OK;
		if ( trySetInputType( m_sinkWriter, m_streamIndex, cand.subtype, width, height, m_fps, cand.fullAttributes, candHr ) )
		{
			m_inputFormat = cand.format;
			m_width = width;
			m_height = height;
			inputConfigured = true;
			vDebug() << "MediaFoundationVideoWriter: accepted input format" << cand.name
			         << ( enableHardwareTransforms ? "with HW" : "with SW" )
			         << "for resolution" << width << "x" << height;
			break;
		}
		lastInputHr = candHr;
	}

	if ( !inputConfigured )
	{
		m_errorString = QStringLiteral( "SetInputMediaType failed (0x%1) for %2x%3 (%4)" )
							.arg( static_cast<quint32>( lastInputHr ), 8, 16, QLatin1Char( '0' ) )
							.arg( width )
							.arg( height )
							.arg( enableHardwareTransforms ? QStringLiteral( "HW" ) : QStringLiteral( "SW" ) );
		m_sinkWriter->Release();
		m_sinkWriter = nullptr;
		return false;
	}

	hr = m_sinkWriter->BeginWriting();
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "BeginWriting failed (0x%1)" )
							.arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		m_sinkWriter->Release();
		m_sinkWriter = nullptr;
		return false;
	}

	return true;
#else
	Q_UNUSED( nativePath )
	Q_UNUSED( codec )
	Q_UNUSED( enableHardwareTransforms )
	Q_UNUSED( width )
	Q_UNUSED( height )
	return false;
#endif
}

bool MediaFoundationVideoWriter::open( const QString& filePath, int width, int height, int fps, int quality )
{
	close();

#ifdef Q_OS_WIN
	auto& mf = MediaFoundationApi::instance();
	if ( !mf.isValid() )
	{
		m_errorString = QStringLiteral( "Media Foundation libraries (mfplat.dll / mfreadwrite.dll) not available" );
		return false;
	}

	if ( width <= 0 || height <= 0 || fps <= 0 )
	{
		m_errorString = QStringLiteral( "Invalid video dimensions or frame rate" );
		return false;
	}

	m_fps = fps;
	m_quality = qBound( 1, quality, 100 );

	// Priority 1: 16-aligned width, even height (macroblock boundary for Intel QuickSync & H.264)
	const int alignedW = ( width + 15 ) & ~15;
	const int alignedH = ( height + 1 ) & ~1;

	// Priority 2: exact even dimensions
	const int evenW = ( width + 1 ) & ~1;
	const int evenH = ( height + 1 ) & ~1;

	HRESULT hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
	m_comInitialized = SUCCEEDED( hr ) || hr == S_FALSE;

	hr = mf.fnStartup( MF_VERSION, MFSTARTUP_NOSOCKET );
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "MFStartup failed (0x%1)" ).arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		close();
		return false;
	}
	m_mfStarted = true;

	if ( mf.fnMFTRegisterLocalByCLSID )
	{
		static const GUID GUID_CColorConvertDMO =
			{ 0x98230571, 0x0087, 0x4204, { 0x97, 0xba, 0x08, 0x3c, 0x53, 0x36, 0x30, 0x7b } };
		static const GUID GUID_MFT_CATEGORY_VIDEO_PROCESSOR =
			{ 0xf79eac7d, 0xe545, 0x4387, { 0xbd, 0xee, 0xd6, 0x47, 0xd7, 0xbd, 0xe4, 0x2a } };
		static const UINT32 MFT_ENUM_FLAG_SYNCMFT = 0x00000001;

		mf.fnMFTRegisterLocalByCLSID(
			GUID_CColorConvertDMO,
			GUID_MFT_CATEGORY_VIDEO_PROCESSOR,
			L"",
			MFT_ENUM_FLAG_SYNCMFT,
			0, NULL, 0, NULL );
	}

	QFileInfo fileInfo( filePath );
	QDir().mkpath( fileInfo.absolutePath() );
	const QString nativePath = QDir::toNativeSeparators( filePath );

	// Build list of resolutions to attempt:
	// 1. Aligned to 16 width (e.g. 1366 -> 1376, 1920 -> 1920)
	// 2. Exact even dimensions (if different from 16-aligned)
	// 3. Fallback standard 1280x720 (720p - guaranteed universal support)
	QList<QPair<int, int>> resolutionsToTry;
	resolutionsToTry.append( qMakePair( alignedW, alignedH ) );
	if ( evenW != alignedW || evenH != alignedH )
	{
		resolutionsToTry.append( qMakePair( evenW, evenH ) );
	}
	if ( alignedW != 1280 || alignedH != 720 )
	{
		resolutionsToTry.append( qMakePair( 1280, 720 ) );
	}

	bool ok = false;

	for ( const auto& res : resolutionsToTry )
	{
		const int testW = res.first;
		const int testH = res.second;

		// 1. Preferred codec with GPU hardware acceleration
		cleanupSinkWriter( nativePath );
		ok = createPipeline( nativePath, m_codec, true /* hardware */, testW, testH );
		if ( ok )
		{
			vDebug() << "MediaFoundationVideoWriter: Pipeline created successfully with HW at"
			         << testW << "x" << testH;
			break;
		}
		vDebug() << "MediaFoundationVideoWriter: HW failed at" << testW << "x" << testH
		         << ":" << m_errorString << "- falling back to CPU/SW";

		// 2. Preferred codec with pure CPU software encoder
		cleanupSinkWriter( nativePath );
		ok = createPipeline( nativePath, m_codec, false /* software */, testW, testH );
		if ( ok )
		{
			vDebug() << "MediaFoundationVideoWriter: Pipeline created successfully with SW at"
			         << testW << "x" << testH;
			break;
		}
		vDebug() << "MediaFoundationVideoWriter: SW failed at" << testW << "x" << testH
		         << ":" << m_errorString;

		// 3. If HEVC requested but unsupported by system, fall back to H.264 (hardware then software)
		if ( m_codec == Codec::H265 )
		{
			cleanupSinkWriter( nativePath );
			ok = createPipeline( nativePath, Codec::H264, true /* hardware */, testW, testH );
			if ( ok )
			{
				m_codec = Codec::H264;
				vDebug() << "MediaFoundationVideoWriter: Pipeline created with H.264 HW fallback at"
				         << testW << "x" << testH;
				break;
			}

			cleanupSinkWriter( nativePath );
			ok = createPipeline( nativePath, Codec::H264, false /* software */, testW, testH );
			if ( ok )
			{
				m_codec = Codec::H264;
				vDebug() << "MediaFoundationVideoWriter: Pipeline created with H.264 SW fallback at"
				         << testW << "x" << testH;
				break;
			}
		}
	}

	if ( !ok )
	{
		cleanupSinkWriter( nativePath );
		close();
		return false;
	}

	m_initialized = true;
	return true;
#else
	Q_UNUSED( filePath )
	Q_UNUSED( width )
	Q_UNUSED( height )
	Q_UNUSED( fps )
	Q_UNUSED( quality )
	m_errorString = QStringLiteral( "Media Foundation is only available on Windows" );
	return false;
#endif
}

bool MediaFoundationVideoWriter::writeFrame( const QImage& frame, qint64 timestampMs )
{
#ifdef Q_OS_WIN
	if ( !m_initialized || !m_sinkWriter )
	{
		m_errorString = QStringLiteral( "Writer is not initialized" );
		return false;
	}

	auto& mf = MediaFoundationApi::instance();
	if ( !mf.isValid() )
	{
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

	IMFMediaBuffer* pBuffer = nullptr;
	HRESULT hr = S_OK;

	if ( m_inputFormat == InputPixelFormat::NV12 )
	{
		const DWORD cbBuffer = static_cast<DWORD>( ( m_width * m_height * 3 ) / 2 );
		hr = mf.fnCreateMemoryBuffer( cbBuffer, &pBuffer );
		if ( FAILED( hr ) || !pBuffer )
		{
			m_errorString = QStringLiteral( "MFCreateMemoryBuffer failed" );
			return false;
		}

		BYTE* pData = nullptr;
		hr = pBuffer->Lock( &pData, NULL, NULL );
		if ( SUCCEEDED( hr ) && pData )
		{
			convertRgb32ToNv12( img, pData, m_width, m_height );
			pBuffer->Unlock();
			pBuffer->SetCurrentLength( cbBuffer );
		}
	}
	else if ( m_inputFormat == InputPixelFormat::YUY2 )
	{
		const DWORD cbBuffer = static_cast<DWORD>( m_width * m_height * 2 );
		hr = mf.fnCreateMemoryBuffer( cbBuffer, &pBuffer );
		if ( FAILED( hr ) || !pBuffer )
		{
			m_errorString = QStringLiteral( "MFCreateMemoryBuffer failed" );
			return false;
		}

		BYTE* pData = nullptr;
		hr = pBuffer->Lock( &pData, NULL, NULL );
		if ( SUCCEEDED( hr ) && pData )
		{
			convertRgb32ToYuy2( img, pData, m_width, m_height );
			pBuffer->Unlock();
			pBuffer->SetCurrentLength( cbBuffer );
		}
	}
	else
	{
		const DWORD cbWidth = static_cast<DWORD>( m_width * 4 );
		const DWORD cbBuffer = cbWidth * static_cast<DWORD>( m_height );

		hr = mf.fnCreateMemoryBuffer( cbBuffer, &pBuffer );
		if ( FAILED( hr ) || !pBuffer )
		{
			m_errorString = QStringLiteral( "MFCreateMemoryBuffer failed" );
			return false;
		}

		BYTE* pData = nullptr;
		hr = pBuffer->Lock( &pData, NULL, NULL );
		if ( SUCCEEDED( hr ) && pData )
		{
			const BYTE* pSrc = img.constBits();
			const int srcBytesPerLine = img.bytesPerLine();
			if ( srcBytesPerLine == static_cast<int>( cbWidth ) )
			{
				memcpy( pData, pSrc, cbBuffer );
			}
			else
			{
				for ( int y = 0; y < m_height; ++y )
				{
					memcpy( pData + ( y * cbWidth ), pSrc + ( y * srcBytesPerLine ), cbWidth );
				}
			}
			pBuffer->Unlock();
			pBuffer->SetCurrentLength( cbBuffer );
		}
	}

	if ( FAILED( hr ) || !pBuffer )
	{
		return false;
	}

	IMFSample* pSample = nullptr;
	hr = mf.fnCreateSample( &pSample );
	if ( SUCCEEDED( hr ) && pSample )
	{
		pSample->AddBuffer( pBuffer );
		const LONGLONG llTime = timestampMs * 10000LL;
		const LONGLONG llDuration = ( 1000LL / m_fps ) * 10000LL;
		pSample->SetSampleTime( llTime );
		pSample->SetSampleDuration( llDuration );

		hr = m_sinkWriter->WriteSample( m_streamIndex, pSample );
		pSample->Release();
	}
	pBuffer->Release();

	return SUCCEEDED( hr );
#else
	Q_UNUSED( frame )
	Q_UNUSED( timestampMs )
	return false;
#endif
}

void MediaFoundationVideoWriter::close()
{
#ifdef Q_OS_WIN
	if ( m_sinkWriter )
	{
		if ( m_initialized )
		{
			m_sinkWriter->Finalize();
		}
		m_sinkWriter->Release();
		m_sinkWriter = nullptr;
	}

	if ( m_mfStarted )
	{
		MediaFoundationApi::instance().fnShutdown();
		m_mfStarted = false;
	}

	if ( m_comInitialized )
	{
		CoUninitialize();
		m_comInitialized = false;
	}

	m_initialized = false;
#endif
}
