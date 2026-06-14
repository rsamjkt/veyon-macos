# AruniVoice: two-way push-to-talk / intercom audio. Needs Qt Multimedia for
# cross-platform microphone capture (QAudioSource) and playback (QAudioSink).
include(BuildVeyonPlugin)

find_package(Qt6 REQUIRED COMPONENTS Multimedia)

build_veyon_plugin(arunivoice
	NAME AruniVoice
	SOURCES
	AruniVoicePlugin.cpp
	AruniVoicePlugin.h
	AudioEngine.cpp
	AudioEngine.h
	VoiceWidget.cpp
	VoiceWidget.h
	arunivoice.qrc
	)

target_link_libraries(arunivoice PRIVATE Qt6::Multimedia)
