/*
 * ScriptPackage.cpp - implementation of ScriptPackage class
 *
 * Part of Veyon Community Addons
 */

#include "ScriptPackage.h"

#include <QDataStream>
#include <QDirIterator>
#include <QFile>

static constexpr quint32 PackageMagic = 0x52534350; // 'RSCP'
static constexpr quint32 PackageVersion = 1;

void ScriptPackage::setEntryScript( const QString& scriptFileName, const QString& scriptContent )
{
	m_entryScriptFileName = sanitizeRelativePath( scriptFileName );
	if ( m_entryScriptFileName.isEmpty() )
	{
		m_entryScriptFileName = QStringLiteral("script.ps1");
	}
	m_scriptContent = scriptContent;
}

bool ScriptPackage::addFile( const QString& absoluteFilePath, const QString& relativeDestinationPath )
{
	QFileInfo fileInfo( absoluteFilePath );
	if ( !fileInfo.exists() || !fileInfo.isFile() )
	{
		return false;
	}

	QFile file( absoluteFilePath );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		return false;
	}

	QByteArray content = file.readAll();
	file.close();

	QString relPath = relativeDestinationPath.isEmpty() ? fileInfo.fileName() : relativeDestinationPath;
	relPath = sanitizeRelativePath( relPath );
	if ( relPath.isEmpty() )
	{
		return false;
	}

	PackageItem item;
	item.relativePath = relPath;
	item.data = content;
	item.size = content.size();
	item.isDirectory = false;

	// Replace existing item if matching relative path
	for ( int i = 0; i < m_items.size(); ++i )
	{
		if ( m_items[i].relativePath == relPath )
		{
			m_items[i] = item;
			return true;
		}
	}

	m_items.append( item );
	return true;
}

bool ScriptPackage::addFolder( const QString& absoluteFolderPath, const QString& relativeDestinationPrefix )
{
	QDir rootDir( absoluteFolderPath );
	if ( !rootDir.exists() )
	{
		return false;
	}

	QString folderName = rootDir.dirName();
	QString basePrefix = relativeDestinationPrefix.isEmpty() ? folderName : relativeDestinationPrefix;
	basePrefix = sanitizeRelativePath( basePrefix );

	QDirIterator it( absoluteFolderPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories );
	while ( it.hasNext() )
	{
		it.next();
		QFileInfo info = it.fileInfo();
		QString relFromRoot = rootDir.relativeFilePath( info.absoluteFilePath() );
		QString itemRelPath = basePrefix.isEmpty() ? relFromRoot : ( basePrefix + QLatin1Char('/') + relFromRoot );
		itemRelPath = sanitizeRelativePath( itemRelPath );

		if ( info.isDir() )
		{
			PackageItem dirItem;
			dirItem.relativePath = itemRelPath;
			dirItem.isDirectory = true;
			dirItem.size = 0;
			m_items.append( dirItem );
		}
		else if ( info.isFile() )
		{
			addFile( info.absoluteFilePath(), itemRelPath );
		}
	}

	return true;
}

void ScriptPackage::removeFile( int index )
{
	if ( index >= 0 && index < m_items.size() )
	{
		m_items.removeAt( index );
	}
}

void ScriptPackage::clearFiles()
{
	m_items.clear();
}

void ScriptPackage::clearAll()
{
	m_entryScriptFileName = QStringLiteral("script.ps1");
	m_scriptContent.clear();
	m_items.clear();
}

qint64 ScriptPackage::totalUncompressedSize() const
{
	qint64 total = m_scriptContent.toUtf8().size();
	for ( const auto& item : m_items )
	{
		total += item.size;
	}
	return total;
}

QByteArray ScriptPackage::serializeAndCompress() const
{
	QByteArray raw;
	QDataStream stream( &raw, QIODevice::WriteOnly );
	stream.setVersion( QDataStream::Qt_6_0 );

	stream << PackageMagic;
	stream << PackageVersion;
	stream << m_entryScriptFileName;
	stream << m_scriptContent;
	stream << qint32( m_items.size() );

	for ( const auto& item : m_items )
	{
		stream << item.relativePath;
		stream << item.isDirectory;
		stream << item.size;
		stream << item.data;
	}

	return qCompress( raw, 6 );
}

bool ScriptPackage::deserializeAndDecompress( const QByteArray& compressedData )
{
	if ( compressedData.isEmpty() )
	{
		return false;
	}

	QByteArray raw = qUncompress( compressedData );
	if ( raw.isEmpty() )
	{
		return false;
	}

	QDataStream stream( &raw, QIODevice::ReadOnly );
	stream.setVersion( QDataStream::Qt_6_0 );

	quint32 magic = 0;
	quint32 version = 0;
	stream >> magic >> version;

	if ( magic != PackageMagic || version != PackageVersion )
	{
		return false;
	}

	stream >> m_entryScriptFileName;
	stream >> m_scriptContent;

	qint32 count = 0;
	stream >> count;
	if ( count < 0 || count > 100000 )
	{
		return false;
	}

	m_items.clear();
	m_items.reserve( count );

	for ( int i = 0; i < count; ++i )
	{
		PackageItem item;
		stream >> item.relativePath;
		stream >> item.isDirectory;
		stream >> item.size;
		stream >> item.data;
		m_items.append( item );
	}

	return true;
}

bool ScriptPackage::extractTo( const QString& destinationDirectory ) const
{
	QDir destDir( destinationDirectory );
	if ( !destDir.exists() && !destDir.mkpath( QStringLiteral(".") ) )
	{
		return false;
	}

	// 1. Write the entry script if provided
	if ( !m_scriptContent.isEmpty() && !m_entryScriptFileName.isEmpty() )
	{
		QString scriptFullPath = destDir.absoluteFilePath( m_entryScriptFileName );
		QFileInfo scriptInfo( scriptFullPath );
		destDir.mkpath( scriptInfo.dir().path() );

		QFile scriptFile( scriptFullPath );
		if ( scriptFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		{
			scriptFile.write( m_scriptContent.toUtf8() );
			scriptFile.close();
		}
		else
		{
			return false;
		}
	}

	// 2. Extract companion files and folders
	for ( const auto& item : m_items )
	{
		QString cleanRel = sanitizeRelativePath( item.relativePath );
		if ( cleanRel.isEmpty() )
		{
			continue;
		}

		QString targetPath = destDir.absoluteFilePath( cleanRel );
		if ( item.isDirectory )
		{
			destDir.mkpath( cleanRel );
		}
		else
		{
			QFileInfo info( targetPath );
			destDir.mkpath( info.dir().path() );

			QFile file( targetPath );
			if ( file.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			{
				file.write( item.data );
				file.close();
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

QString ScriptPackage::sanitizeRelativePath( const QString& path )
{
	QString sanitized = QDir::cleanPath( path );
	while ( sanitized.startsWith( QLatin1Char('/') ) || sanitized.startsWith( QLatin1Char('\\') ) )
	{
		sanitized.remove( 0, 1 );
	}

	// Prevent directory traversal attacks
	if ( sanitized.contains( QStringLiteral("..") ) || QDir::isAbsolutePath( sanitized ) || sanitized.contains( QLatin1Char(':') ) )
	{
		return QString();
	}

	return sanitized;
}
