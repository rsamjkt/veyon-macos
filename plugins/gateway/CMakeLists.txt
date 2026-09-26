include(BuildVeyonPlugin)

# Aruni Gateway: a client PC keeps an outbound connection to the Aruni Relay so
# that Masters on other networks (e.g. the Android app on mobile data) can reach
# the computers of this LAN
find_package(Qt${QT_MAJOR_VERSION} COMPONENTS WebSockets QUIET)
if(NOT Qt${QT_MAJOR_VERSION}WebSockets_FOUND)
	message(WARNING "Qt WebSockets not found - skipping the Aruni Gateway plugin")
	return()
endif()

set(qrcodegen_DIR ${CMAKE_SOURCE_DIR}/3rdparty/qrcodegen)
# the vendored QR library reports errors with exceptions
set_source_files_properties(${qrcodegen_DIR}/qrcodegen.cpp PROPERTIES COMPILE_OPTIONS "-fexceptions" SKIP_UNITY_BUILD_INCLUSION TRUE)

# roaming laptops report their foreground application (activity history) - the
# platform code is shared with the Application Monitoring plugin
set(gateway_platform_sources)
if(VEYON_BUILD_APPLE)
	set(gateway_platform_sources ${CMAKE_SOURCE_DIR}/plugins/appmonitoring/MacApplicationList.mm)
elseif(VEYON_BUILD_WINDOWS)
	set(gateway_platform_sources ${CMAKE_SOURCE_DIR}/plugins/appmonitoring/WinApplicationList.cpp)
endif()

build_veyon_plugin(gateway
	NAME Gateway
	SOURCES
	ActivityLog.cpp
	ActivityLog.h
	Notifier.cpp
	Notifier.h
	MonitoringCollector.cpp
	MonitoringCollector.h
	TimelapseView.cpp
	TimelapseView.h
	GatewayPlugin.cpp
	GatewayPlugin.h
	GatewayConfigurationPage.cpp
	GatewayConfigurationPage.h
	GatewayService.cpp
	GatewayService.h
	GatewaySession.cpp
	GatewaySession.h
	GatewayState.cpp
	GatewayState.h
	RoamingAgent.cpp
	RoamingAgent.h
	TunnelEndpoint.cpp
	TunnelEndpoint.h
	${qrcodegen_DIR}/qrcodegen.cpp
	${gateway_platform_sources}
	)

target_include_directories(gateway PRIVATE ${qrcodegen_DIR})
target_link_libraries(gateway PRIVATE Qt${QT_MAJOR_VERSION}::WebSockets)

if(gateway_platform_sources)
	target_include_directories(gateway PRIVATE ${CMAKE_SOURCE_DIR}/plugins/appmonitoring)
	set_source_files_properties(${gateway_platform_sources} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
	if(VEYON_BUILD_APPLE)
		target_link_libraries(gateway PRIVATE "-framework AppKit" "-framework Foundation")
	elseif(VEYON_BUILD_WINDOWS)
		target_link_libraries(gateway PRIVATE -lversion -luser32)
		target_compile_definitions(gateway PRIVATE ARUNI_HAVE_APPLICATION_LIST)
	endif()
	if(VEYON_BUILD_APPLE)
		target_compile_definitions(gateway PRIVATE ARUNI_HAVE_APPLICATION_LIST)
	endif()
endif()
