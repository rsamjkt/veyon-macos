include(BuildVeyonPlugin)

build_veyon_plugin(android-platform
	NAME AndroidPlatform
	SOURCES
	AndroidPlatformPlugin.cpp
	AndroidPlatformPlugin.h
	AndroidPlatformFunctions.cpp
	AndroidPlatformFunctions.h
	)

target_link_libraries(android-platform PRIVATE log)
