include(BuildVeyonPlugin)

build_veyon_plugin(mac-platform
	NAME MacPlatform
	SOURCES
	MacPlatformPlugin.cpp
	MacPlatformPlugin.h
	MacCoreFunctions.cpp
	MacCoreFunctions.h
	MacFilesystemFunctions.cpp
	MacFilesystemFunctions.h
	MacInputDeviceFunctions.cpp
	MacInputDeviceFunctions.h
	MacKeyboardShortcutTrapper.h
	MacNetworkFunctions.cpp
	MacNetworkFunctions.h
	MacServiceFunctions.cpp
	MacServiceFunctions.h
	MacSessionFunctions.cpp
	MacSessionFunctions.h
	MacUserFunctions.cpp
	MacUserFunctions.h
	)

# macOS system frameworks used by the platform plugin
target_link_libraries(mac-platform PRIVATE
	"-framework CoreFoundation"
	"-framework CoreGraphics"
	"-framework ApplicationServices"
	"-framework IOKit"
	)
