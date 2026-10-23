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
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#endif

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
	return true;
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

	HRESULT hr = MFStartup( MF_VERSION, MFSTARTUP_NOSOCKET );
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "MFStartup failed (0x%1)" ).arg( static_cast<quint32>( hr ), 8, 16, QLatin1Char( '0' ) );
		return false;
	}

	QFileInfo fileInfo( filePath );
	QDir().mkpath( fileInfo.absolutePath() );

	IMFAttributes* pAttributes = nullptr;
	hr = MFCreateAttributes( &pAttributes, 2 );
	if ( SUCCEEDED( hr ) && pAttributes )
	{
		pAttributes->SetUINT32( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE );
		pAttributes->SetUINT32( MF_SINK_WRITER_DISABLE_THROTTLING, TRUE );
	}

	const QString nativePath = QDir::toNativeSeparators( filePath );
	hr = MFCreateSinkWriterFromURL( reinterpret_cast<LPCWSTR>( nativePath.utf16() ),
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
		MFShutdown();
		CoUninitialize();
		return false;
	}

	// Output MediaType: H.264 video
	IMFMediaType* pMediaTypeOut = nullptr;
	hr = MFCreateMediaType( &pMediaTypeOut );
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "MFCreateMediaType failed" );
		close();
		return false;
	}

	pMediaTypeOut->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	pMediaTypeOut->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_H264 );
	pMediaTypeOut->SetUINT32( MF_MT_AVG_BITRATE, bitrateKbps * 1000 );
	pMediaTypeOut->SetUINT32( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize( pMediaTypeOut, MF_MT_FRAME_SIZE, static_cast<UINT32>( m_width ), static_cast<UINT32>( m_height ) );
	MFSetAttributeRatio( pMediaTypeOut, MF_MT_FRAME_RATE, static_cast<UINT32>( m_fps ), 1 );
	MFSetAttributeRatio( pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

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
	hr = MFCreateMediaType( &pMediaTypeIn );
	if ( FAILED( hr ) )
	{
		m_errorString = QStringLiteral( "MFCreateMediaType for input failed" );
		close();
		return false;
	}

	pMediaTypeIn->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	pMediaTypeIn->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
	pMediaTypeIn->SetUINT32( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize( pMediaTypeIn, MF_MT_FRAME_SIZE, static_cast<UINT32>( m_width ), static_cast<UINT32>( m_height ) );
	MFSetAttributeRatio( pMediaTypeIn, MF_MT_FRAME_RATE, static_cast<UINT32>( m_fps ), 1 );
	MFSetAttributeRatio( pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

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
	HRESULT hr = MFCreateMemoryBuffer( cbBuffer, &pBuffer );
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
	hr = MFCreateSample( &pSample );
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
			m_sinkWriter->DoFinalize();
		}
		m_sinkWriter->Release();
		m_sinkWriter = nullptr;
	}

	if ( m_initialized )
	{
		MFShutdown();
		CoUninitialize();
		m_initialized = false;
	}
#endif
}
