/*
 * ScriptPackage.h - Package encapsulation for scripts and companion files/folders
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QFileInfo>
#include <QDir>

class ScriptPackage
{
public:
	struct PackageItem
	{
		QString relativePath;
		QByteArray data;
		qint64 size{0};
		bool isDirectory{false};
	};

	ScriptPackage() = default;

	void setEntryScript( const QString& scriptFileName, const QString& scriptContent = QString() );
	QString entryScriptFileName() const { return m_entryScriptFileName; }
	QString scriptContent() const { return m_scriptContent; }

	bool addFile( const QString& absoluteFilePath, const QString& relativeDestinationPath = QString() );
	bool addFolder( const QString& absoluteFolderPath, const QString& relativeDestinationPrefix = QString() );
	void removeFile( int index );
	void clearFiles();
	void clearAll();

	const QVector<PackageItem>& items() const { return m_items; }
	int itemCount() const { return m_items.size(); }
	qint64 totalUncompressedSize() const;

	QByteArray serializeAndCompress() const;
	bool deserializeAndDecompress( const QByteArray& compressedData );

	bool extractTo( const QString& destinationDirectory ) const;

	static QString sanitizeRelativePath( const QString& path );

private:
	QString m_entryScriptFileName{ QStringLiteral("script.ps1") };
	QString m_scriptContent;
	QVector<PackageItem> m_items;
};
