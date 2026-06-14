if(VEYON_BUILD_WINDOWS)
	add_subdirectory(ultravnc-builtin)
endif()

if(VEYON_BUILD_LINUX)
	add_subdirectory(x11vnc-builtin)
endif()

if(VEYON_BUILD_APPLE)
	add_subdirectory(mac)
endif()

add_subdirectory(external)
add_subdirectory(headless)
