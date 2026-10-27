/*
 * ScriptRunner.cpp - implementation of ScriptRunner class
 *
 * Part of Veyon Community Addons
 */

#include "ScriptRunner.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QUuid>
#include <QFileInfo>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

QString ScriptRunner::getStagingDirectory()
{
#ifdef Q_OS_WIN
	QString base = QStringLiteral("C:/Windows/Temp");
	if ( !QDir( base ).exists() )
	{
		base = QDir::tempPath();
	}
#else
	QString base = QStringLiteral("/tmp");
	if ( !QDir( base ).exists() )
	{
		base = QDir::tempPath();
	}
#endif
	QString uniqueName = QStringLiteral("VeyonDeploy_") + QUuid::createUuid().toString( QUuid::WithoutBraces );
	return QDir::toNativeSeparators( base + QLatin1Char('/') + uniqueName );
}

ScriptRunner::InterpreterType ScriptRunner::detectInterpreter( const QString& scriptFileName, InterpreterType preferred )
{
	if ( preferred != InterpreterType::Auto )
	{
		return preferred;
	}

	QString lower = scriptFileName.toLower();
	if ( lower.endsWith( QStringLiteral(".ps1") ) )
	{
		return InterpreterType::PowerShell;
	}
	if ( lower.endsWith( QStringLiteral(".bat") ) || lower.endsWith( QStringLiteral(".cmd") ) )
	{
		return InterpreterType::BatchCmd;
	}
	if ( lower.endsWith( QStringLiteral(".vbs") ) )
	{
		return InterpreterType::VbScript;
	}
	if ( lower.endsWith( QStringLiteral(".exe") ) )
	{
		return InterpreterType::Executable;
	}

	// Default fallback to PowerShell
	return InterpreterType::PowerShell;
}

ScriptRunner::ExecutionResult ScriptRunner::execute( const ScriptPackage& package, const ExecutionOptions& options )
{
	ExecutionResult result;
	QElapsedTimer timer;
	timer.start();

	// 1. Create temporary staging directory
	QString stagingDir = getStagingDirectory();
	if ( !QDir().mkpath( stagingDir ) )
	{
		result.errorMessage = QStringLiteral("Failed to create staging directory: ") + stagingDir;
		result.executionTimeMs = timer.elapsed();
		return result;
	}

	// 2. Extract package items into staging directory
	if ( !package.extractTo( stagingDir ) )
	{
		result.errorMessage = QStringLiteral("Failed to extract package files into: ") + stagingDir;
		if ( options.cleanupAfter )
		{
			QDir( stagingDir ).removeRecursively();
		}
		result.executionTimeMs = timer.elapsed();
		return result;
	}

	// 3. Determine entry script and interpreter
	QString scriptName = package.entryScriptFileName();
	QString scriptFullPath = QDir( stagingDir ).absoluteFilePath( scriptName );
	InterpreterType interpreter = detectInterpreter( scriptName, options.interpreter );

	QString program;
	QStringList arguments;

	switch ( interpreter )
	{
	case InterpreterType::PowerShell:
		program = QStringLiteral("powershell.exe");
		arguments << QStringLiteral("-NoProfile")
				  << QStringLiteral("-NonInteractive")
				  << QStringLiteral("-ExecutionPolicy") << QStringLiteral("Bypass")
				  << QStringLiteral("-WindowStyle") << QStringLiteral("Hidden")
				  << QStringLiteral("-File") << scriptFullPath;
		break;

	case InterpreterType::BatchCmd:
		program = QStringLiteral("cmd.exe");
		arguments << QStringLiteral("/c") << scriptFullPath;
		break;

	case InterpreterType::VbScript:
		program = QStringLiteral("cscript.exe");
		arguments << QStringLiteral("//NoLogo") << scriptFullPath;
		break;

	case InterpreterType::Executable:
		program = scriptFullPath;
		break;

	default:
		program = QStringLiteral("powershell.exe");
		arguments << QStringLiteral("-NoProfile")
				  << QStringLiteral("-ExecutionPolicy") << QStringLiteral("Bypass")
				  << QStringLiteral("-File") << scriptFullPath;
		break;
	}

	if ( !options.arguments.trimmed().isEmpty() )
	{
		// Parse space-separated arguments, respecting quotes
		static const QRegularExpression argRegex( QStringLiteral("([^\"]\\S*|\".+?\")\\s*") );
		auto matchIt = argRegex.globalMatch( options.arguments.trimmed() );
		while ( matchIt.hasNext() )
		{
			auto match = matchIt.next();
			QString arg = match.captured( 1 );
			if ( arg.startsWith( QLatin1Char('"') ) && arg.endsWith( QLatin1Char('"') ) && arg.size() >= 2 )
			{
				arg = arg.mid( 1, arg.size() - 2 );
			}
			arguments << arg;
		}
	}

	// 4. Setup process
	QProcess process;
	process.setWorkingDirectory( stagingDir );
	process.setProcessChannelMode( QProcess::MergedChannels );

	// Inject environment variable pointing to the package directory
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.insert( QStringLiteral("VEYON_DEPLOY_DIR"), stagingDir );
	process.setProcessEnvironment( env );

#ifdef Q_OS_WIN
	process.setCreateProcessArgumentsModifier( []( QProcess::CreateProcessArguments* args ) {
		args->flags |= CREATE_NO_WINDOW;
	} );
#endif

	// 5. Run process
	process.start( program, arguments );
	if ( !process.waitForStarted( 10000 ) )
	{
		result.errorMessage = QStringLiteral("Failed to start process: ") + process.errorString();
		if ( options.cleanupAfter )
		{
			QDir( stagingDir ).removeRecursively();
		}
		result.executionTimeMs = timer.elapsed();
		return result;
	}

	int timeoutMs = options.timeoutSeconds > 0 ? ( options.timeoutSeconds * 1000 ) : -1;
	bool finished = process.waitForFinished( timeoutMs );

	if ( !finished )
	{
		result.timedOut = true;
		result.errorMessage = QStringLiteral("Execution timed out after ") + QString::number( options.timeoutSeconds ) + QStringLiteral(" seconds.");
		process.kill();
		process.waitForFinished( 3000 );
	}
	else
	{
		result.exitCode = process.exitCode();
		result.success = ( result.exitCode == 0 );
	}

	result.output = QString::fromLocal8Bit( process.readAll() );
	result.executionTimeMs = timer.elapsed();

	// 6. Cleanup if requested
	if ( options.cleanupAfter )
	{
		QDir( stagingDir ).removeRecursively();
	}

	return result;
}
