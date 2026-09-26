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

build_veyon_plugin(gateway
	NAME Gateway
	SOURCES
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
	)

target_include_directories(gateway PRIVATE ${qrcodegen_DIR})
target_link_libraries(gateway PRIVATE Qt${QT_MAJOR_VERSION}::WebSockets)
