# AruniControl Mobile - touch-friendly Master for Android (also builds on
# desktop platforms for quick UI development with -DWITH_MOBILE=ON)

set(MASTER_SRC ${CMAKE_SOURCE_DIR}/master/src)

qt_policy(SET QTP0001 NEW)
if(QT_KNOWN_POLICY_QTP0002)
	qt_policy(SET QTP0002 NEW)
endif()

# the Master's data models are shared with the desktop Master; its widgets are not
set(mobile_MASTER_SOURCES
	${MASTER_SRC}/CheckableItemProxyModel.cpp
	${MASTER_SRC}/ComputerControlListModel.cpp
	${MASTER_SRC}/ComputerManager.cpp
	${MASTER_SRC}/ComputerMonitoringModel.cpp
	${MASTER_SRC}/KExtraColumnsProxyModel.cpp
	${MASTER_SRC}/NetworkObjectFilterProxyModel.cpp
	${MASTER_SRC}/NetworkObjectOverlayDataModel.cpp
	${MASTER_SRC}/NetworkObjectTreeModel.cpp
	${MASTER_SRC}/UserConfig.cpp
	${MASTER_SRC}/VeyonMaster.cpp
	)

file(GLOB mobile_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB mobile_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml)
file(GLOB mobile_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.svg)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# place the QML files at the module root so they share its qmldir (singletons)
foreach(qml_file ${mobile_QML})
	get_filename_component(qml_name ${qml_file} NAME)
	set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

qt_add_executable(arunicontrol-mobile
	${mobile_SOURCES}
	${mobile_MASTER_SOURCES}
	${CMAKE_SOURCE_DIR}/master/resources/master.qrc
	)

set_source_files_properties(${mobile_MASTER_SOURCES} PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)

qt_add_qml_module(arunicontrol-mobile
	URI AruniControl
	VERSION 1.0
	QML_FILES ${mobile_QML}
	RESOURCES ${mobile_ICONS} images/logo.svg fonts/PlusJakartaSans.ttf
	)

target_compile_definitions(arunicontrol-mobile PRIVATE ARUNICONTROL_MOBILE)
target_include_directories(arunicontrol-mobile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${MASTER_SRC})
find_package(Qt6 REQUIRED COMPONENTS WebSockets)
target_link_libraries(arunicontrol-mobile PRIVATE
	veyon-core
	Qt6::Quick
	Qt6::QuickControls2
	Qt6::WebSockets
	)
set_default_target_properties(arunicontrol-mobile)

if(VEYON_BUILD_ANDROID)
	# QtAndroidPrivate::startActivity() for the VPN consent dialog
	find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
	target_link_libraries(arunicontrol-mobile PRIVATE Qt6::CorePrivate)

	get_property(VEYON_ANDROID_PLUGINS GLOBAL PROPERTY VEYON_ANDROID_PLUGINS)
	add_dependencies(arunicontrol-mobile ${VEYON_ANDROID_PLUGINS})

	# Qt mangles generator expressions in QT_ANDROID_EXTRA_LIBS - use plain paths
	get_target_property(core_dir veyon-core BINARY_DIR)
	get_target_property(vncclient_dir vncclient BINARY_DIR)
	set(ANDROID_EXTRA_LIBS
		${core_dir}/libveyon-core.so
		${vncclient_dir}/libvncclient.so
		${DEPS_PREFIX}/lib/libqca-qt6.so
		${DEPS_PREFIX}/lib/qca-qt6/crypto/libqca-ossl.so
		${DEPS_PREFIX}/lib/libssl.so
		${DEPS_PREFIX}/lib/libcrypto.so
		)
	foreach(plugin ${VEYON_ANDROID_PLUGINS})
		get_target_property(plugin_dir ${plugin} BINARY_DIR)
		list(APPEND ANDROID_EXTRA_LIBS ${plugin_dir}/libveyon-plugin-${plugin}.so)
	endforeach()

	math(EXPR ANDROID_VERSION_CODE "${VERSION_MAJOR} * 10000 + ${VERSION_MINOR} * 100 + ${VERSION_PATCH}")

	set_target_properties(arunicontrol-mobile PROPERTIES
		OUTPUT_NAME AruniControl
		QT_ANDROID_PACKAGE_NAME id.arunika.arunicontrol
		QT_ANDROID_APP_NAME AruniControl
		QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
		QT_ANDROID_EXTRA_LIBS "${ANDROID_EXTRA_LIBS}"
		# extract native libraries so PluginManager can list the plugins
		QT_ANDROID_LEGACY_PACKAGING TRUE
		QT_ANDROID_MIN_SDK_VERSION 28
		QT_ANDROID_TARGET_SDK_VERSION 35
		QT_ANDROID_VERSION_NAME "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
		QT_ANDROID_VERSION_CODE ${ANDROID_VERSION_CODE}
		)
else()
	set_target_properties(arunicontrol-mobile PROPERTIES OUTPUT_NAME arunicontrol-mobile)
endif()
