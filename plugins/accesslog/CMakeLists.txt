include(BuildVeyonPlugin)

# Serves the access log of a computer (written by veyon-server) to Masters,
# the app and the Aruni Gateway
build_veyon_plugin(accesslog
	NAME AccessLog
	SOURCES
	AccessLogFeaturePlugin.cpp
	AccessLogFeaturePlugin.h
	AccessLogDialog.cpp
	AccessLogDialog.h
	accesslog.qrc
	)
