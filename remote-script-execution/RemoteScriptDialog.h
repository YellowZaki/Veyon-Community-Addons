/*
 * RemoteScriptDialog.h - dialog for configuring and launching remote scripts
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QDialog>
#include <QMap>
#include <QUuid>
#include "ComputerControlInterface.h"
#include "ScriptPackage.h"
#include "ScriptRunner.h"

namespace Ui {
class RemoteScriptDialog;
}

class RemoteScriptExecutionFeaturePlugin;

class RemoteScriptDialog : public QDialog
{
	Q_OBJECT

public:
	explicit RemoteScriptDialog( RemoteScriptExecutionFeaturePlugin* plugin,
								 const ComputerControlInterfaceList& computerControlInterfaces,
								 QWidget* parent = nullptr );
	~RemoteScriptDialog() override;

	void handleExecutionResult( const ComputerControlInterface::Pointer& computer,
								QUuid executionId,
								int exitCode,
								const QString& output,
								bool success,
								qint64 executionTimeMs,
								const QString& errorMessage );

private Q_SLOTS:
	void onLoadScriptFileClicked();
	void onAttachFileClicked();
	void onAttachFolderClicked();
	void onRemoveFileClicked();
	void onClearFilesClicked();
	void onExecuteClicked();
	void onResultTableSelectionChanged();
	void onCopyLogClicked();
	void onInterpreterChanged( int index );

private:
	struct ComputerStatus
	{
		QString name;
		QString host;
		QString state;
		int exitCode{ -1 };
		qint64 durationMs{ 0 };
		QString logOutput;
		bool finished{ false };
		bool success{ false };
	};

	void refreshFilesTable();
	void refreshResultsTable();
	QString formatSize( qint64 bytes ) const;
	QString defaultScriptExtension() const;

	Ui::RemoteScriptDialog* ui;
	RemoteScriptExecutionFeaturePlugin* m_plugin;
	ComputerControlInterfaceList m_computers;
	ScriptPackage m_package;
	QUuid m_currentExecutionId;
	QMap<ComputerControlInterface::Pointer, ComputerStatus> m_computerStatuses;
	int m_finishedCount{ 0 };
};
