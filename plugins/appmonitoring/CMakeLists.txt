# Application monitoring: lists running GUI apps on the client.
# Implemented on macOS (NSWorkspace) and Windows (Win32 EnumWindows); Android
# (AruniControl Mobile) only needs the master side.
if(VEYON_BUILD_APPLE OR VEYON_BUILD_WINDOWS OR VEYON_BUILD_ANDROID)

include(BuildVeyonPlugin)

set(appmonitoring_platform_sources)
if(VEYON_BUILD_APPLE)
	set(appmonitoring_platform_sources MacApplicationList.mm)
elseif(VEYON_BUILD_WINDOWS)
	set(appmonitoring_platform_sources WinApplicationList.cpp)
else()
	set(appmonitoring_platform_sources NullApplicationList.cpp)
endif()

build_veyon_plugin(appmonitoring
	NAME ApplicationMonitoring
	SOURCES
	ApplicationMonitoringPlugin.cpp
	ApplicationMonitoringPlugin.h
	ApplicationListDialog.cpp
	ApplicationListDialog.h
	ApplicationList.h
	${appmonitoring_platform_sources}
	appmonitoring.qrc
	)

if(VEYON_BUILD_APPLE)
	target_link_libraries(appmonitoring PRIVATE "-framework AppKit" "-framework Foundation")
	set_source_files_properties(MacApplicationList.mm PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
elseif(VEYON_BUILD_WINDOWS)
	target_link_libraries(appmonitoring PRIVATE -lversion -luser32)
	set_source_files_properties(WinApplicationList.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endif()

endif()
