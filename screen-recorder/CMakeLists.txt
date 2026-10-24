#
# CMakeLists.txt for Screen Recorder Addon for Veyon
# Part of Veyon Community Addons
#

cmake_minimum_required(VERSION 3.18)
project(screenrecorder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
	ScreenRecorderFeaturePlugin.cpp
	ScreenRecorderFeaturePlugin.h
	ScreenRecorderConfiguration.h
	ScreenRecorderConfigurationPage.cpp
	ScreenRecorderConfigurationPage.h
	ScreenRecorderConfigurationPage.ui
	ScreenRecorderControlWidget.cpp
	ScreenRecorderControlWidget.h
	ScreenRecorderControlWidget.ui
	ScreenRecorderEncodingWorker.cpp
	ScreenRecorderEncodingWorker.h
	VideoWriter.h
	MediaFoundationVideoWriter.cpp
	MediaFoundationVideoWriter.h
	AviVideoWriter.cpp
	AviVideoWriter.h
	screenrecorder.qrc
)

if(COMMAND build_veyon_plugin)
	# In-tree build with Veyon
	build_veyon_plugin(screenrecorder
		NAME ScreenRecorder
		SOURCES ${SOURCES}
	)
	if(WIN32)
		target_link_libraries(screenrecorder PRIVATE ole32 oleaut32 uuid)
		if(MINGW)
			target_link_options(screenrecorder PRIVATE "-Wl,--allow-multiple-definition")
		endif()
	endif()
else()
	# Out-of-tree / standalone build
	find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTORCC ON)
	set(CMAKE_AUTOUIC ON)

	add_library(screenrecorder MODULE ${SOURCES})
	set_target_properties(screenrecorder PROPERTIES PREFIX "")

	find_path(VEYON_INCLUDE_DIR
		NAMES VeyonCore.h
		PATHS
			${VEYON_SOURCE_DIR}/core/src
			${CMAKE_CURRENT_SOURCE_DIR}/../veyon-core-upstream/core/src
			${CMAKE_CURRENT_SOURCE_DIR}/../veyon-core/core/src
			${CMAKE_CURRENT_SOURCE_DIR}/../veyon/core/src
			"C:/Program Files/Veyon/sdk/include"
	)

	target_include_directories(screenrecorder PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/compat
	)

	if(VEYON_INCLUDE_DIR)
		target_include_directories(screenrecorder PRIVATE ${VEYON_INCLUDE_DIR})
	endif()

	target_link_libraries(screenrecorder PRIVATE
		Qt6::Core
		Qt6::Gui
		Qt6::Widgets
		Qt6::Network
	)

	set(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a" ".a" ".lib" ".dll")

	find_library(VEYON_CORE_LIB
		NAMES veyon-core libveyon-core veyon-core.dll
		PATHS
			${CMAKE_CURRENT_SOURCE_DIR}/../veyon-bin
			${CMAKE_CURRENT_BINARY_DIR}/../veyon-bin
			${VEYON_SOURCE_DIR}
			"C:/Program Files/Veyon"
			"C:/Program Files/Veyon/sdk/lib"
	)

	if(NOT VEYON_CORE_LIB)
		find_file(VEYON_CORE_DLL
			NAMES veyon-core.dll
			PATHS
				${CMAKE_CURRENT_SOURCE_DIR}/../veyon-bin
				${CMAKE_CURRENT_BINARY_DIR}/../veyon-bin
				${VEYON_SOURCE_DIR}
				"C:/Program Files/Veyon"
		)
		if(VEYON_CORE_DLL)
			set(VEYON_CORE_LIB "${VEYON_CORE_DLL}")
		endif()
	endif()

	if(VEYON_CORE_LIB)
		message(STATUS "Found Veyon Core library: ${VEYON_CORE_LIB}")
		target_link_libraries(screenrecorder PRIVATE "${VEYON_CORE_LIB}")
	else()
		message(FATAL_ERROR "Veyon Core library NOT found! Cannot link plugin.")
	endif()

	if(WIN32)
		target_link_libraries(screenrecorder PRIVATE ole32 oleaut32 uuid)
		if(MINGW)
			target_link_options(screenrecorder PRIVATE "-Wl,--allow-multiple-definition")
		endif()
	endif()
endif()
