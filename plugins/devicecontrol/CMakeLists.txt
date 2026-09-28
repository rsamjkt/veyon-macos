include(BuildVeyonPlugin)

# Blocks USB storage devices and printing on the selected computers (Windows)
build_veyon_plugin(devicecontrol
	NAME DeviceControl
	SOURCES
	DeviceControlFeaturePlugin.cpp
	DeviceControlFeaturePlugin.h
	devicecontrol.qrc
	)
