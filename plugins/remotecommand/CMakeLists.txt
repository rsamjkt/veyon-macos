include(BuildVeyonPlugin)

# Runs a command/script on the selected computers and shows each output
build_veyon_plugin(remotecommand
	NAME RemoteCommand
	SOURCES
	RemoteCommandFeaturePlugin.cpp
	RemoteCommandFeaturePlugin.h
	RemoteCommandDialog.cpp
	RemoteCommandDialog.h
	remotecommand.qrc
	)
