/*
 * MediaFoundationVideoWriter.cpp - Windows Media Foundation H.264 / MP4 writer
 *
 * Part of Veyon Community Addons
 */

#include "MediaFoundationVideoWriter.h"

#include <QDir>
#include <QFileInfo>

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
static const GUID GUID_MF_MT_MAJOR_TYPE = { 0x48eba18e, 0xf8c9, 0x4687, { 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xe9, 0x6d, 0x4b } };
static const GUID GUID_MF_MT_SUBTYPE = { 0xf7eac761, 0xb352, 0x4e50, { 0xbf, 0x75, 0x16, 0x72, 0x04, 0xe6, 0x0b, 0x70 } };
static const GUID GUID_MFMediaType_Video = { 0x73646976, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID GUID_MFVideoFormat_H264 = { 0x34363248, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID GUID_MFVideoFormat_RGB32 = { 0x00000016, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID GUID_MF_MT_AVG_BITRATE = { 0x20360262, 0x07f0, 0x4fa6, { 0x9e, 0x22, 0x13, 0xe7, 0xec, 0x92, 0x3e, 0x08 } };
static const GUID GUID_MF_MT_FRAME_SIZE = { 0x1652c33d, 0xd6b2, 0x4012, { 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d } };
static const GUID GUID_MF_MT_FRAME_RATE = { 0xc450743c, 0x7953, 0x4e18, { 0xb2, 0x9e, 0x26, 0xe4, 0x6e, 0xcd, 0x13, 0x71 } };
static const GUID GUID_MF_MT_PIXEL_ASPECT_RATIO = { 0xc63764b2, 0xd808, 0x4ae8, { 0x84, 0xa0, 0xc1, 0x8a, 0x16, 0x24, 0x4c, 0x4c } };
static const GUID GUID_MF_MT_INTERLACE_MODE = { 0xe2724447, 0xda04, 0x4223, { 0x85, 0x6f, 0x00, 0x02, 0xa0, 0x8e, 0xac, 0xa6 } };
static const GUID GUID_MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS = { 0xa634a91c, 0x822b, 0x41b9, { 0xa4, 0x94, 0x4d, 0xe4, 0x64, 0x36, 0x12, 0xb0 } };
static const GUID GUID_MF_SINK_WRITER_DISABLE_THROTTLING = { 0x08b845d8, 0x2b74, 0x4af2, { 0xbf, 0x66, 0x03, 0x22, 0x5a, 0x74, 0xb4, 0x4f } };

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

MediaFoundationVideoWriter::MediaFoundationVideoWriter()
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

bool MediaFoundationVideoWriter::open( const QString& filePath, int width, int height, int fps, int bitrateKbps )
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

	// Dimensions must be even for H.264
	m_width = ( width + 1 ) & ~1;
	m_height = ( height + 1 ) & ~1;
	m_fps = fps;

	CoInitializeEx( NULL, COINIT_MULTITHREADED );

	HRESULT hr = mf.fnStartup( MF_VERSION, MFSTARTUP_NOSOCKET );
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "MFStartup failed (0x%1)" ).arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		CoUninitialize();
		return false;
	}

	QFileInfo fileInfo( filePath );
	QDir().mkpath( fileInfo.absolutePath() );

	IMFAttributes* pAttributes = nullptr;
	hr = mf.fnCreateAttributes( &pAttributes, 2 );
	if ( SUCCEEDED( hr ) && pAttributes )
	{
		pAttributes->SetUINT32( GUID_MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE );
		pAttributes->SetUINT32( GUID_MF_SINK_WRITER_DISABLE_THROTTLING, TRUE );
	}

	const QString nativePath = QDir::toNativeSeparators( filePath );
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
		m_errorString = QStringLiteral( "MFCreateSinkWriterFromURL failed (0x%1)" ).arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		mf.fnShutdown();
		CoUninitialize();
		return false;
	}

	// Output MediaType: H.264 video
	IMFMediaType* pMediaTypeOut = nullptr;
	hr = mf.fnCreateMediaType( &pMediaTypeOut );
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "MFCreateMediaType for output failed" );
		close();
		return false;
	}

	pMediaTypeOut->SetGUID( GUID_MF_MT_MAJOR_TYPE, GUID_MFMediaType_Video );
	pMediaTypeOut->SetGUID( GUID_MF_MT_SUBTYPE, GUID_MFVideoFormat_H264 );
	pMediaTypeOut->SetUINT32( GUID_MF_MT_AVG_BITRATE, bitrateKbps * 1000 );
	pMediaTypeOut->SetUINT32( GUID_MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	setAttrSize( pMediaTypeOut, GUID_MF_MT_FRAME_SIZE, static_cast<UINT32>( m_width ), static_cast<UINT32>( m_height ) );
	setAttrRatio( pMediaTypeOut, GUID_MF_MT_FRAME_RATE, static_cast<UINT32>( m_fps ), 1 );
	setAttrRatio( pMediaTypeOut, GUID_MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	hr = m_sinkWriter->AddStream( pMediaTypeOut, &m_streamIndex );
	pMediaTypeOut->Release();
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "AddStream (H.264) failed (0x%1)" ).arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		close();
		return false;
	}

	// Input MediaType: RGB32
	IMFMediaType* pMediaTypeIn = nullptr;
	hr = mf.fnCreateMediaType( &pMediaTypeIn );
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "MFCreateMediaType for input failed" );
		close();
		return false;
	}

	pMediaTypeIn->SetGUID( GUID_MF_MT_MAJOR_TYPE, GUID_MFMediaType_Video );
	pMediaTypeIn->SetGUID( GUID_MF_MT_SUBTYPE, GUID_MFVideoFormat_RGB32 );
	pMediaTypeIn->SetUINT32( GUID_MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	setAttrSize( pMediaTypeIn, GUID_MF_MT_FRAME_SIZE, static_cast<UINT32>( m_width ), static_cast<UINT32>( m_height ) );
	setAttrRatio( pMediaTypeIn, GUID_MF_MT_FRAME_RATE, static_cast<UINT32>( m_fps ), 1 );
	setAttrRatio( pMediaTypeIn, GUID_MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	hr = m_sinkWriter->SetInputMediaType( m_streamIndex, pMediaTypeIn, NULL );
	pMediaTypeIn->Release();
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "SetInputMediaType (RGB32) failed (0x%1)" ).arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		close();
		return false;
	}

	hr = m_sinkWriter->BeginWriting();
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "BeginWriting failed (0x%1)" ).arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		close();
		return false;
	}

	m_initialized = true;
	return true;
#else
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
	if ( img.format() != QImage::Format_RGB32 && img.format() != QImage::Format_ARGB32_Premultiplied )
	{
		img = img.convertToFormat( QImage::Format_RGB32 );
	}

	const DWORD cbWidth = static_cast<DWORD>( m_width * 4 );
	const DWORD cbBuffer = cbWidth * static_cast<DWORD>( m_height );

	IMFMediaBuffer* pBuffer = nullptr;
	HRESULT hr = mf.fnCreateMemoryBuffer( cbBuffer, &pBuffer );
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

	if ( m_initialized )
	{
		MediaFoundationApi::instance().fnShutdown();
		CoUninitialize();
		m_initialized = false;
	}
#endif
}
