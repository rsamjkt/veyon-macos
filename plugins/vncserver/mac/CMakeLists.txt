include(BuildVeyonPlugin)

get_property(HAVE_LIBVNCCLIENT GLOBAL PROPERTY HAVE_LIBVNCCLIENT)
if(HAVE_LIBVNCCLIENT)
	find_package(LibVNCServer 0.9.8)
endif()

if(LibVNCServer_FOUND)

	build_veyon_plugin(mac-vnc-server
		NAME MacVncServer
		SOURCES
		MacVncServer.cpp
		MacVncServer.h
		MacVncInput.cpp
		MacVncInput.h
		MacScreenCapture.mm
		MacScreenCapture.h
		)

	target_link_libraries(mac-vnc-server PRIVATE
		LibVNC::LibVNCServer
		"-framework CoreGraphics"
		"-framework CoreFoundation"
		"-framework CoreVideo"
		"-framework ApplicationServices"
		"-framework ScreenCaptureKit"
		)

	target_compile_options(mac-vnc-server PRIVATE -Wno-parentheses -Wno-deprecated-declarations)

	# the Objective-C++ capture backend uses ARC; ScreenCaptureKit has no plain-C API
	set_source_files_properties(MacScreenCapture.mm PROPERTIES
		COMPILE_OPTIONS "-fobjc-arc;-fexceptions")
	# .mm files must not be swept into the C++ unity build
	set_source_files_properties(MacScreenCapture.mm PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

endif()
