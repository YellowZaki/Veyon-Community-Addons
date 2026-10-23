/*
 * AviVideoWriter.cpp - Motion-JPEG in AVI video writer implementation
 *
 * Part of Veyon Community Addons
 */

#include "AviVideoWriter.h"

#include <QBuffer>
#include <QDataStream>
#include <QFileInfo>
#include <QDir>

static inline quint32 fourCc( char a, char b, char c, char d )
{
	return static_cast<quint32>(static_cast<quint8>(a)) |
		   (static_cast<quint32>(static_cast<quint8>(b)) << 8) |
		   (static_cast<quint32>(static_cast<quint8>(c)) << 16) |
		   (static_cast<quint32>(static_cast<quint8>(d)) << 24);
}

AviVideoWriter::AviVideoWriter()
{
}

AviVideoWriter::~AviVideoWriter()
{
	close();
}

bool AviVideoWriter::isOpen() const
{
	return m_file.isOpen();
}

QString AviVideoWriter::errorString() const
{
	return m_errorString;
}

bool AviVideoWriter::open( const QString& filePath, int width, int height, int fps, int bitrateKbps )
{
	close();

	Q_UNUSED( bitrateKbps )

	if ( width <= 0 || height <= 0 || fps <= 0 )
	{
		m_errorString = QStringLiteral( "Invalid video dimensions or frame rate" );
		return false;
	}

	QFileInfo fileInfo( filePath );
	QDir().mkpath( fileInfo.absolutePath() );

	m_file.setFileName( filePath );
	if ( !m_file.open( QIODevice::ReadWrite | QIODevice::Truncate ) )
	{
		m_errorString = m_file.errorString();
		return false;
	}

	m_width = width;
	m_height = height;
	m_fps = fps;
	m_quality = 85;
	m_frameCount = 0;
	m_index.clear();

	writeAviHeaders();

	return true;
}

void AviVideoWriter::writeAviHeaders()
{
	QDataStream stream( &m_file );
	stream.setByteOrder( QDataStream::LittleEndian );

	// RIFF Header
	stream << fourCc( 'R', 'I', 'F', 'F' );
	m_riffSizeOffset = m_file.pos();
	stream << static_cast<quint32>( 0 ); // Will be updated on close
	stream << fourCc( 'A', 'V', 'I', ' ' );

	// hdrl LIST
	stream << fourCc( 'L', 'I', 'S', 'T' );
	const quint32 hdrlSize = 4 + 8 + 56 + ( 4 + 8 + 56 + 8 + 40 ); // 'hdrl' + avih + strl list
	stream << hdrlSize;
	stream << fourCc( 'h', 'd', 'r', 'l' );

	// avih chunk (Main AVI Header: 56 bytes)
	stream << fourCc( 'a', 'v', 'i', 'h' );
	stream << static_cast<quint32>( 56 );
	const quint32 microSecPerFrame = 1000000 / m_fps;
	stream << microSecPerFrame;
	stream << static_cast<quint32>( m_width * m_height * 3 ); // MaxBytesPerSec
	stream << static_cast<quint32>( 0 ); // PaddingGranularity
	stream << static_cast<quint32>( 0x910 ); // AVIF_HASINDEX | AVIF_ISINTERLEAVED | AVIF_TRUSTCKTYPE
	m_avihFramesOffset = m_file.pos();
	stream << static_cast<quint32>( 0 ); // TotalFrames (update on close)
	stream << static_cast<quint32>( 0 ); // InitialFrames
	stream << static_cast<quint32>( 1 ); // Streams count (1 video stream)
	stream << static_cast<quint32>( m_width * m_height * 3 ); // SuggestedBufferSize
	stream << static_cast<quint32>( m_width );
	stream << static_cast<quint32>( m_height );
	stream << static_cast<quint32>( 0 ); // Reserved
	stream << static_cast<quint32>( 0 );
	stream << static_cast<quint32>( 0 );
	stream << static_cast<quint32>( 0 );

	// strl LIST
	stream << fourCc( 'L', 'I', 'S', 'T' );
	const quint32 strlSize = 4 + ( 8 + 56 ) + ( 8 + 40 ); // 'strl' + strh + strf
	stream << strlSize;
	stream << fourCc( 's', 't', 'r', 'l' );

	// strh chunk (Stream Header: 56 bytes)
	stream << fourCc( 's', 't', 'r', 'h' );
	stream << static_cast<quint32>( 56 );
	stream << fourCc( 'v', 'i', 'd', 's' ); // Stream type: video
	stream << fourCc( 'M', 'J', 'P', 'G' ); // Handler: Motion JPEG
	stream << static_cast<quint32>( 0 ); // Flags
	stream << static_cast<quint16>( 0 ); // Priority
	stream << static_cast<quint16>( 0 ); // Language
	stream << static_cast<quint32>( 0 ); // InitialFrames
	stream << static_cast<quint32>( 1 ); // Scale
	stream << static_cast<quint32>( m_fps ); // Rate
	stream << static_cast<quint32>( 0 ); // Start
	m_strhLengthOffset = m_file.pos();
	stream << static_cast<quint32>( 0 ); // Length in frames (update on close)
	stream << static_cast<quint32>( m_width * m_height * 3 ); // SuggestedBufferSize
	stream << static_cast<quint32>( 10000 ); // Quality
	stream << static_cast<quint32>( 0 ); // SampleSize
	stream << static_cast<qint16>( 0 ); // Frame rcFrame Left
	stream << static_cast<qint16>( 0 ); // Frame rcFrame Top
	stream << static_cast<qint16>( m_width ); // Frame rcFrame Right
	stream << static_cast<qint16>( m_height ); // Frame rcFrame Bottom

	// strf chunk (Stream Format - BITMAPINFOHEADER: 40 bytes)
	stream << fourCc( 's', 't', 'r', 'f' );
	stream << static_cast<quint32>( 40 );
	stream << static_cast<quint32>( 40 ); // biSize
	stream << static_cast<qint32>( m_width ); // biWidth
	stream << static_cast<qint32>( m_height ); // biHeight
	stream << static_cast<quint16>( 1 ); // biPlanes
	stream << static_cast<quint16>( 24 ); // biBitCount
	stream << fourCc( 'M', 'J', 'P', 'G' ); // biCompression
	stream << static_cast<quint32>( m_width * m_height * 3 ); // biSizeImage
	stream << static_cast<qint32>( 0 ); // biXPelsPerMeter
	stream << static_cast<qint32>( 0 ); // biYPelsPerMeter
	stream << static_cast<quint32>( 0 ); // biClrUsed
	stream << static_cast<quint32>( 0 ); // biClrImportant

	// movi LIST
	stream << fourCc( 'L', 'I', 'S', 'T' );
	m_moviSizeOffset = m_file.pos();
	stream << static_cast<quint32>( 0 ); // Will be updated on close
	stream << fourCc( 'm', 'o', 'v', 'i' );

	m_moviDataOffset = m_file.pos();
}

bool AviVideoWriter::writeFrame( const QImage& frame, qint64 timestampMs )
{
	Q_UNUSED( timestampMs )

	if ( !m_file.isOpen() )
	{
		m_errorString = QStringLiteral( "File is not open" );
		return false;
	}

	QImage img = frame;
	if ( img.size() != QSize( m_width, m_height ) )
	{
		img = img.scaled( m_width, m_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
	}

	QByteArray jpegBuffer;
	QBuffer buffer( &jpegBuffer );
	buffer.open( QIODevice::WriteOnly );
	if ( !img.save( &buffer, "JPEG", m_quality ) )
	{
		m_errorString = QStringLiteral( "Failed to compress frame to JPEG" );
		return false;
	}

	const quint32 jpegSize = static_cast<quint32>( jpegBuffer.size() );
	const quint32 offsetFromMovi = static_cast<quint32>( m_file.pos() - m_moviDataOffset );

	QDataStream stream( &m_file );
	stream.setByteOrder( QDataStream::LittleEndian );

	// Write chunk '00dc' (stream 00, uncompressed/compressed video frame)
	stream << fourCc( '0', '0', 'd', 'c' );
	stream << jpegSize;
	m_file.write( jpegBuffer );

	// Chunks must be aligned to 2-byte boundary
	if ( jpegSize % 2 != 0 )
	{
		const char pad = 0;
		m_file.write( &pad, 1 );
	}

	AviIndexEntry entry;
	entry.ckid = fourCc( '0', '0', 'd', 'c' );
	entry.flags = 0x10; // AVIIF_KEYFRAME (every JPEG is a keyframe)
	entry.chunkOffset = offsetFromMovi;
	entry.chunkLength = jpegSize;
	m_index.append( entry );

	m_frameCount++;
	return true;
}

void AviVideoWriter::close()
{
	if ( !m_file.isOpen() )
	{
		return;
	}

	updateAviHeaders();
	m_file.close();
}

void AviVideoWriter::updateAviHeaders()
{
	QDataStream stream( &m_file );
	stream.setByteOrder( QDataStream::LittleEndian );

	// Write idx1 chunk
	const qint64 idx1StartPos = m_file.pos();
	stream << fourCc( 'i', 'd', 'x', '1' );
	const quint32 idx1Size = static_cast<quint32>( m_index.size() * sizeof( AviIndexEntry ) );
	stream << idx1Size;

	for ( const auto& entry : m_index )
	{
		stream << entry.ckid;
		stream << entry.flags;
		stream << entry.chunkOffset;
		stream << entry.chunkLength;
	}

	const qint64 totalFileSize = m_file.pos();

	// Update RIFF size: totalFileSize - 8
	m_file.seek( m_riffSizeOffset );
	stream << static_cast<quint32>( totalFileSize - 8 );

	// Update total frames in avih
	m_file.seek( m_avihFramesOffset );
	stream << m_frameCount;

	// Update stream length in strh
	m_file.seek( m_strhLengthOffset );
	stream << m_frameCount;

	// Update movi size: bytes between 'movi' FourCC and start of 'idx1' chunk
	const quint32 moviSize = static_cast<quint32>( ( idx1StartPos - m_moviDataOffset ) + 4 );
	m_file.seek( m_moviSizeOffset );
	stream << moviSize;

	m_file.flush();
}
