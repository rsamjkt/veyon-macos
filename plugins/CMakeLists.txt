file(GLOB plugins RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

# plugins not yet ported / lacking required dependencies on macOS
if(VEYON_BUILD_APPLE)
	set(VEYON_EXCLUDED_PLUGINS ldap)
endif()

# On the AruniControl Windows build we optionally skip LDAP (no AD/OpenLDAP
# toolchain on the MSYS2 CI runner; classroom deployments use the built-in and
# network-discovery directories instead). Enable with -DVEYON_ARUNICONTROL_SKIP_LDAP=ON.
if(VEYON_BUILD_WINDOWS AND VEYON_ARUNICONTROL_SKIP_LDAP)
	list(APPEND VEYON_EXCLUDED_PLUGINS ldap)
endif()

foreach(plugin ${plugins})
	if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${plugin})
		if(plugin IN_LIST VEYON_EXCLUDED_PLUGINS)
			message(STATUS "Skipping plugin '${plugin}' on this platform")
		else()
			add_subdirectory(${plugin})
		endif()
	endif()
endforeach()
