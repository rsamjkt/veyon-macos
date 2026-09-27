include(BuildVeyonPlugin)

# Installs a program (.msi/.exe/.pkg) silently on the selected computers and
# removes installed programs
build_veyon_plugin(softwaredeploy
	NAME SoftwareDeploy
	SOURCES
	SoftwareDeployFeaturePlugin.cpp
	SoftwareDeployFeaturePlugin.h
	SoftwareDeployDialog.cpp
	SoftwareDeployDialog.h
	softwaredeploy.qrc
	)
