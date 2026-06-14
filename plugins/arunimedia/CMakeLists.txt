include(BuildVeyonPlugin)

set(arunimedia_platform_sources)
if(VEYON_BUILD_WINDOWS)
	set(arunimedia_platform_sources WinAudioMute.cpp WinAudioMute.h)
endif()

build_veyon_plugin(arunimedia
	NAME AruniMedia
	SOURCES
	AruniMediaFeaturePlugin.cpp
	AruniMediaFeaturePlugin.h
	${arunimedia_platform_sources}
	arunimedia.qrc
	)

if(VEYON_BUILD_WINDOWS)
	# WASAPI (IMMDeviceEnumerator / IAudioEndpointVolume) lives in ole32
	target_link_libraries(arunimedia PRIVATE -lole32)
	set_source_files_properties(WinAudioMute.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endif()
