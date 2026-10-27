/*
 * RemoteScriptDialog.cpp - implementation of RemoteScriptDialog class
 *
 * Part of Veyon Community Addons
 */

#include "RemoteScriptDialog.h"
#include "ui_RemoteScriptDialog.h"
#include "RemoteScriptExecutionFeaturePlugin.h"

#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>

RemoteScriptDialog::RemoteScriptDialog( RemoteScriptExecutionFeaturePlugin* plugin,
										const ComputerControlInterfaceList& computerControlInterfaces,
										QWidget* parent ) :
	QDialog( parent ),
	ui( new Ui::RemoteScriptDialog ),
	m_plugin( plugin ),
	m_computers( computerControlInterfaces )
{
	ui->setupUi( this );

	// Headers and table configurations
	ui->tblFiles->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
	ui->tblFiles->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	ui->tblFiles->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
	ui->tblFiles->setSelectionBehavior( QAbstractItemView::SelectRows );

	ui->tblResults->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	ui->tblResults->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	ui->tblResults->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Stretch );
	ui->tblResults->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
	ui->tblResults->horizontalHeader()->setSectionResizeMode( 4, QHeaderView::ResizeToContents );
	ui->tblResults->setSelectionBehavior( QAbstractItemView::SelectRows );

	ui->lblTargetComputers->setText( tr( "Equipos destino seleccionados: %1 equipos" ).arg( m_computers.size() ) );
	ui->progressBar->setMaximum( m_computers.isEmpty() ? 1 : m_computers.size() );
	ui->progressBar->setValue( 0 );

	// Initialize statuses
	for ( const auto& comp : m_computers )
	{
		ComputerStatus status;
		status.name = comp->computer().displayName().isEmpty() ? comp->computerName() : comp->computer().displayName();
		status.host = comp->computer().hostName();
		status.state = tr( "Listo" );
		m_computerStatuses[comp] = status;
	}

	refreshResultsTable();

	// Connect signals and slots
	connect( ui->btnLoadScriptFile, &QPushButton::clicked, this, &RemoteScriptDialog::onLoadScriptFileClicked );
	connect( ui->btnAttachFile, &QPushButton::clicked, this, &RemoteScriptDialog::onAttachFileClicked );
	connect( ui->btnAttachFolder, &QPushButton::clicked, this, &RemoteScriptDialog::onAttachFolderClicked );
	connect( ui->btnRemoveFile, &QPushButton::clicked, this, &RemoteScriptDialog::onRemoveFileClicked );
	connect( ui->btnClearFiles, &QPushButton::clicked, this, &RemoteScriptDialog::onClearFilesClicked );
	connect( ui->btnExecute, &QPushButton::clicked, this, &RemoteScriptDialog::onExecuteClicked );
	connect( ui->btnClose, &QPushButton::clicked, this, &QDialog::accept );
	connect( ui->btnCopyLog, &QPushButton::clicked, this, &RemoteScriptDialog::onCopyLogClicked );
	connect( ui->comboInterpreter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteScriptDialog::onInterpreterChanged );
	connect( ui->tblResults, &QTableWidget::itemSelectionChanged, this, &RemoteScriptDialog::onResultTableSelectionChanged );
}

RemoteScriptDialog::~RemoteScriptDialog()
{
	delete ui;
}

QString RemoteScriptDialog::formatSize( qint64 bytes ) const
{
	if ( bytes < 1024 )
	{
		return QString::number( bytes ) + QStringLiteral(" B");
	}
	if ( bytes < 1024 * 1024 )
	{
		return QString::number( bytes / 1024.0, 'f', 1 ) + QStringLiteral(" KB");
	}
	return QString::number( bytes / ( 1024.0 * 1024.0 ), 'f', 2 ) + QStringLiteral(" MB");
}

QString RemoteScriptDialog::defaultScriptExtension() const
{
	switch ( ui->comboInterpreter->currentIndex() )
	{
	case 0: return QStringLiteral(".ps1");
	case 1: return QStringLiteral(".bat");
	case 2: return QStringLiteral(".vbs");
	case 3: return QStringLiteral(".exe");
	default: return QStringLiteral(".ps1");
	}
}

void RemoteScriptDialog::onInterpreterChanged( int index )
{
	Q_UNUSED( index );
	// If the user hasn't loaded an external file, update default script name
	if ( m_package.entryScriptFileName().startsWith( QStringLiteral("script.") ) )
	{
		m_package.setEntryScript( QStringLiteral("script") + defaultScriptExtension() );
	}
}

void RemoteScriptDialog::onLoadScriptFileClicked()
{
	QString filter = tr( "Scripts (*.ps1 *.bat *.cmd *.vbs *.exe);;PowerShell (*.ps1);;Batch (*.bat *.cmd);;Todos los archivos (*.*)" );
	QString filePath = QFileDialog::getOpenFileName( this, tr( "Cargar script principal" ), QString(), filter );

	if ( filePath.isEmpty() )
	{
		return;
	}

	QFileInfo fileInfo( filePath );
	QFile file( filePath );
	if ( file.open( QIODevice::ReadOnly ) )
	{
		QString content = QString::fromUtf8( file.readAll() );
		ui->txtScriptEditor->setPlainText( content );
		ui->lblLoadedFile->setText( tr( "Archivo cargado: %1 (%2)" ).arg( fileInfo.fileName(), formatSize( fileInfo.size() ) ) );

		QString ext = fileInfo.suffix().toLower();
		if ( ext == QStringLiteral("ps1") )
		{
			ui->comboInterpreter->setCurrentIndex( 0 );
		}
		else if ( ext == QStringLiteral("bat") || ext == QStringLiteral("cmd") )
		{
			ui->comboInterpreter->setCurrentIndex( 1 );
		}
		else if ( ext == QStringLiteral("vbs") )
		{
			ui->comboInterpreter->setCurrentIndex( 2 );
		}
		else if ( ext == QStringLiteral("exe") )
		{
			ui->comboInterpreter->setCurrentIndex( 3 );
		}

		m_package.setEntryScript( fileInfo.fileName(), content );
	}
	else
	{
		QMessageBox::warning( this, tr( "Error" ), tr( "No se pudo leer el archivo seleccionado." ) );
	}
}

void RemoteScriptDialog::onAttachFileClicked()
{
	QStringList files = QFileDialog::getOpenFileNames( this, tr( "Seleccionar archivos adjuntos" ) );
	if ( files.isEmpty() )
	{
		return;
	}

	for ( const auto& file : files )
	{
		m_package.addFile( file );
	}

	refreshFilesTable();
}

void RemoteScriptDialog::onAttachFolderClicked()
{
	QString folder = QFileDialog::getExistingDirectory( this, tr( "Seleccionar carpeta adjunta" ) );
	if ( folder.isEmpty() )
	{
		return;
	}

	m_package.addFolder( folder );
	refreshFilesTable();
}

void RemoteScriptDialog::onRemoveFileClicked()
{
	int currentRow = ui->tblFiles->currentRow();
	if ( currentRow >= 0 && currentRow < m_package.itemCount() )
	{
		m_package.removeFile( currentRow );
		refreshFilesTable();
	}
}

void RemoteScriptDialog::onClearFilesClicked()
{
	if ( m_package.itemCount() == 0 )
	{
		return;
	}

	if ( QMessageBox::question( this, tr( "Confirmar" ), tr( "¿Deseas eliminar todos los archivos adjuntos?" ) ) == QMessageBox::Yes )
	{
		m_package.clearFiles();
		refreshFilesTable();
	}
}

void RemoteScriptDialog::refreshFilesTable()
{
	ui->tblFiles->setRowCount( 0 );
	const auto& items = m_package.items();

	for ( int i = 0; i < items.size(); ++i )
	{
		const auto& item = items[i];
		ui->tblFiles->insertRow( i );

		auto itemPath = new QTableWidgetItem( item.relativePath );
		auto itemType = new QTableWidgetItem( item.isDirectory ? tr( "Carpeta" ) : tr( "Archivo" ) );
		auto itemSize = new QTableWidgetItem( item.isDirectory ? QStringLiteral("-") : formatSize( item.size ) );

		ui->tblFiles->setItem( i, 0, itemPath );
		ui->tblFiles->setItem( i, 1, itemType );
		ui->tblFiles->setItem( i, 2, itemSize );
	}

	ui->lblTotalSize->setText( tr( "Tamaño del paquete: %1" ).arg( formatSize( m_package.totalUncompressedSize() ) ) );
}

void RemoteScriptDialog::refreshResultsTable()
{
	ui->tblResults->setRowCount( 0 );
	int row = 0;

	for ( auto it = m_computerStatuses.begin(); it != m_computerStatuses.end(); ++it, ++row )
	{
		ui->tblResults->insertRow( row );

		const auto& st = it.value();
		auto itemName = new QTableWidgetItem( st.name );
		auto itemHost = new QTableWidgetItem( st.host );
		auto itemState = new QTableWidgetItem( st.state );
		auto itemCode = new QTableWidgetItem( st.exitCode >= 0 ? QString::number( st.exitCode ) : QStringLiteral("-") );
		auto itemDur = new QTableWidgetItem( st.durationMs > 0 ? QStringLiteral("%1 ms").arg( st.durationMs ) : QStringLiteral("-") );

		if ( st.finished )
		{
			if ( st.success )
			{
				itemState->setForeground( QBrush( QColor( 46, 125, 50 ) ) ); // Dark Green
			}
			else
			{
				itemState->setForeground( QBrush( QColor( 198, 40, 40 ) ) ); // Red
			}
		}

		ui->tblResults->setItem( row, 0, itemName );
		ui->tblResults->setItem( row, 1, itemHost );
		ui->tblResults->setItem( row, 2, itemState );
		ui->tblResults->setItem( row, 3, itemCode );
		ui->tblResults->setItem( row, 4, itemDur );
	}
}

void RemoteScriptDialog::onExecuteClicked()
{
	QString scriptText = ui->txtScriptEditor->toPlainText().trimmed();
	if ( scriptText.isEmpty() )
	{
		QMessageBox::warning( this, tr( "Aviso" ), tr( "Por favor, escribe o carga un script antes de ejecutar." ) );
		ui->tabWidget->setCurrentIndex( 0 );
		return;
	}

	if ( m_computers.isEmpty() )
	{
		QMessageBox::warning( this, tr( "Aviso" ), tr( "No hay equipos destino seleccionados." ) );
		return;
	}

	QString scriptName = m_package.entryScriptFileName();
	if ( scriptName.isEmpty() || scriptName == QStringLiteral("script.ps1") )
	{
		scriptName = QStringLiteral("script") + defaultScriptExtension();
	}
	m_package.setEntryScript( scriptName, scriptText );

	// Build options
	ScriptRunner::ExecutionOptions options;
	options.interpreter = static_cast<ScriptRunner::InterpreterType>( ui->comboInterpreter->currentIndex() );
	options.runAsSystem = ui->chkRunAsSystem->isChecked();
	options.timeoutSeconds = ui->spinTimeout->value();
	options.cleanupAfter = ui->chkCleanup->isChecked();
	options.arguments = ui->txtScriptArguments->text().trimmed();

	m_currentExecutionId = QUuid::createUuid();
	m_finishedCount = 0;
	ui->progressBar->setMaximum( m_computers.size() );
	ui->progressBar->setValue( 0 );

	// Reset status for each computer
	for ( auto it = m_computerStatuses.begin(); it != m_computerStatuses.end(); ++it )
	{
		it.value().state = tr( "Enviando y ejecutando..." );
		it.value().exitCode = -1;
		it.value().durationMs = 0;
		it.value().logOutput.clear();
		it.value().finished = false;
		it.value().success = false;
	}
	refreshResultsTable();

	// Switch to results tab
	ui->tabWidget->setCurrentIndex( 3 );
	ui->btnExecute->setEnabled( false );

	// Serialize package and dispatch via plugin
	QByteArray packageData = m_package.serializeAndCompress();
	m_plugin->dispatchExecution( m_computers, m_currentExecutionId, packageData, options );
}

void RemoteScriptDialog::handleExecutionResult( const ComputerControlInterface::Pointer& computer,
												QUuid executionId,
												int exitCode,
												const QString& output,
												bool success,
												qint64 executionTimeMs,
												const QString& errorMessage )
{
	if ( executionId != m_currentExecutionId && !m_currentExecutionId.isNull() )
	{
		return;
	}

	if ( m_computerStatuses.contains( computer ) )
	{
		auto& st = m_computerStatuses[computer];
		st.exitCode = exitCode;
		st.durationMs = executionTimeMs;
		st.finished = true;
		st.success = success;

		if ( !errorMessage.isEmpty() )
		{
			st.state = tr( "Error: %1" ).arg( errorMessage );
			st.logOutput = errorMessage + QStringLiteral("\n\n") + output;
		}
		else if ( success )
		{
			st.state = tr( "Completado con éxito (Exit Code 0)" );
			st.logOutput = output;
		}
		else
		{
			st.state = tr( "Finalizado con error (Exit Code %1)" ).arg( exitCode );
			st.logOutput = output;
		}

		m_finishedCount++;
		ui->progressBar->setValue( m_finishedCount );
	}

	refreshResultsTable();

	if ( m_finishedCount >= m_computers.size() )
	{
		ui->btnExecute->setEnabled( true );
	}

	// Update log viewer if the selected computer matches
	onResultTableSelectionChanged();
}

void RemoteScriptDialog::onResultTableSelectionChanged()
{
	int currentRow = ui->tblResults->currentRow();
	if ( currentRow < 0 || currentRow >= m_computers.size() )
	{
		return;
	}

	const auto& comp = m_computers.at( currentRow );
	if ( m_computerStatuses.contains( comp ) )
	{
		const auto& st = m_computerStatuses[comp];
		ui->txtLogOutput->setPlainText( st.logOutput.isEmpty() ? tr( "(Sin salida de consola)" ) : st.logOutput );
		ui->lblLogTitle->setText( tr( "<b>Salida de consola de %1:</b>" ).arg( st.name ) );
	}
}

void RemoteScriptDialog::onCopyLogClicked()
{
	QString text = ui->txtLogOutput->toPlainText();
	if ( !text.isEmpty() )
	{
		QGuiApplication::clipboard()->setText( text );
		QMessageBox::information( this, tr( "Copiado" ), tr( "Salida copiada al portapapeles." ) );
	}
}
