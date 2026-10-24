/*
 * ScreenRecorderConfiguration.h - configuration values for Screen Recorder
 *
 * Part of Veyon Community Addons
 */

#pragma once

#include <QStandardPaths>
#include "VeyonConfiguration.h"
#include "Configuration/Proxy.h"

#define FOREACH_SCREENRECORDER_CONFIG_PROPERTY(OP) \
	OP( ScreenRecorderConfiguration, m_configuration, QString, outputDirectory, setOutputDirectory, "OutputDirectory", "ScreenRecorder", QString(), Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, bool, saveToComputerSubdirectories, setSaveToComputerSubdirectories, "SaveToComputerSubdirectories", "ScreenRecorder", true, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, int, maxRecordingsPerComputer, setMaxRecordingsPerComputer, "MaxRecordingsPerComputer", "ScreenRecorder", 3, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, bool, useFixedResolution, setUseFixedResolution, "UseFixedResolution", "ScreenRecorder", false, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, int, resolutionWidth, setResolutionWidth, "ResolutionWidth", "ScreenRecorder", 1920, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, int, resolutionHeight, setResolutionHeight, "ResolutionHeight", "ScreenRecorder", 1080, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, bool, keepAspectRatio, setKeepAspectRatio, "KeepAspectRatio", "ScreenRecorder", true, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, bool, useFixedFramerate, setUseFixedFramerate, "UseFixedFramerate", "ScreenRecorder", true, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, int, framesPerSecond, setFramesPerSecond, "FramesPerSecond", "ScreenRecorder", 15, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, int, format, setFormat, "Format", "ScreenRecorder", 0, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, int, quality, setQuality, "Quality", "ScreenRecorder", 70, Configuration::Property::Flag::Standard ) \
	OP( ScreenRecorderConfiguration, m_configuration, bool, notifyUsers, setNotifyUsers, "NotifyUsers", "ScreenRecorder", false, Configuration::Property::Flag::Standard )

DECLARE_CONFIG_PROXY(ScreenRecorderConfiguration, FOREACH_SCREENRECORDER_CONFIG_PROPERTY)

namespace ScreenRecorderUtil {

inline QString resolvedOutputDirectory( const ScreenRecorderConfiguration& config )
{
	const QString dir = config.outputDirectory().trimmed();
	if ( !dir.isEmpty() )
	{
		return dir;
	}

	QString path = QStandardPaths::writableLocation( QStandardPaths::MoviesLocation );
	if ( path.isEmpty() )
	{
		path = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation );
	}
	return path + QStringLiteral( "/Veyon Recordings" );
}

} // namespace ScreenRecorderUtil
