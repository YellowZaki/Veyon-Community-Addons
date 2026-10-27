/*
 * ScriptRunner.h - Executes scripts and deployments on client machines
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QElapsedTimer>
#include "ScriptPackage.h"

class ScriptRunner
{
public:
	enum class InterpreterType
	{
		Auto = 0,
		PowerShell,
		BatchCmd,
		VbScript,
		Executable
	};

	struct ExecutionOptions
	{
		InterpreterType interpreter{ InterpreterType::Auto };
		bool runAsSystem{ true };
		int timeoutSeconds{ 60 };
		bool cleanupAfter{ true };
		QString arguments;
	};

	struct ExecutionResult
	{
		int exitCode{ -1 };
		QString output;
		qint64 executionTimeMs{ 0 };
		bool success{ false };
		bool timedOut{ false };
		QString errorMessage;
	};

	static ExecutionResult execute( const ScriptPackage& package, const ExecutionOptions& options );

private:
	static InterpreterType detectInterpreter( const QString& scriptFileName, InterpreterType preferred );
	static QString getStagingDirectory();
};
