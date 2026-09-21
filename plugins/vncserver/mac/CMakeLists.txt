include(BuildVeyonPlugin)

build_veyon_plugin(mac-vnc-server
	NAME MacVncServer
	SOURCES
	MacVncServer.cpp
	MacVncServer.h
	MacVncConfiguration.h
	MacVncInput.cpp
	MacVncInput.h
	MacVncCursor.mm
	MacVncCursor.h
	MacScreenCapture.mm
	MacScreenCapture.h
	)

if(WITH_BUNDLED_LIBVNC)
	target_link_libraries(mac-vnc-server PRIVATE vncserver)
else()
	target_link_libraries(mac-vnc-server PRIVATE LibVNC::LibVNCServer)
endif()

target_link_libraries(mac-vnc-server PRIVATE
	"-framework AppKit"
	"-framework CoreGraphics"
	"-framework CoreFoundation"
	"-framework CoreVideo"
	"-framework ApplicationServices"
	"-framework CoreMedia"
	"-framework ScreenCaptureKit"
	)

target_compile_options(mac-vnc-server PRIVATE -Wno-parentheses -Wno-deprecated-declarations)

# the Objective-C++ sources use ARC; neither ScreenCaptureKit nor AppKit has a
# plain-C API
set_source_files_properties(MacScreenCapture.mm MacVncCursor.mm PROPERTIES
	COMPILE_OPTIONS "-fobjc-arc;-fexceptions")
# .mm files must not be swept into the C++ unity build
set_source_files_properties(MacScreenCapture.mm MacVncCursor.mm PROPERTIES
	SKIP_UNITY_BUILD_INCLUSION ON)
