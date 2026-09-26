include(BuildVeyonPlugin)

# Blocks websites on selected computers (hosts file + browser DNS policies)
build_veyon_plugin(sitefilter
	NAME SiteFilter
	SOURCES
	SiteFilterFeaturePlugin.cpp
	SiteFilterFeaturePlugin.h
	SiteFilterDialog.cpp
	SiteFilterDialog.h
	sitefilter.qrc
	)
