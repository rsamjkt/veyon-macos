# uses NSWorkspace (AppKit) - macOS only
if(VEYON_BUILD_APPLE)

include(BuildVeyonPlugin)

build_veyon_plugin(appmonitoring
	NAME ApplicationMonitoring
	SOURCES
	ApplicationMonitoringPlugin.cpp
	ApplicationMonitoringPlugin.h
	ApplicationListDialog.cpp
	ApplicationListDialog.h
	MacApplicationList.mm
	MacApplicationList.h
	appmonitoring.qrc
	)

target_link_libraries(appmonitoring PRIVATE "-framework AppKit" "-framework Foundation")
set_source_files_properties(MacApplicationList.mm PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

endif()
