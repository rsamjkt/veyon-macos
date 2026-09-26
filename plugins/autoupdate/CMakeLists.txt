include(BuildVeyonPlugin)

# Automatic updates of AruniControl on Windows and macOS: the AruniControl
# server checks the download website, verifies and installs new releases
build_veyon_plugin(autoupdate
	NAME AutoUpdate
	SOURCES
	AutoUpdatePlugin.cpp
	AutoUpdatePlugin.h
	AutoUpdateConfigurationPage.cpp
	AutoUpdateConfigurationPage.h
	UpdateState.cpp
	UpdateState.h
	Updater.cpp
	Updater.h
	)
