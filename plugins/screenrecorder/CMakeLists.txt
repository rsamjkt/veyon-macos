# The screen recorder uses AVFoundation, so it is built on macOS only.
if(VEYON_BUILD_APPLE)

include(BuildVeyonPlugin)

build_veyon_plugin(screenrecorder
	NAME ScreenRecorder
	SOURCES
	ScreenRecorderFeaturePlugin.cpp
	ScreenRecorderFeaturePlugin.h
	ScreenRecording.cpp
	ScreenRecording.h
	MacVideoWriter.mm
	MacVideoWriter.h
	screenrecorder.qrc
	)

target_link_libraries(screenrecorder PRIVATE
	"-framework AVFoundation"
	"-framework CoreMedia"
	"-framework CoreVideo"
	"-framework Foundation"
	)

set_source_files_properties(MacVideoWriter.mm PROPERTIES
	COMPILE_OPTIONS "-fobjc-arc;-fexceptions")
set_source_files_properties(MacVideoWriter.mm PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

endif()
