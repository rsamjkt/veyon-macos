include(BuildVeyonPlugin)

build_veyon_plugin(chat
	NAME Chat
	SOURCES
	ChatFeaturePlugin.cpp
	ChatFeaturePlugin.h
	ChatWidget.cpp
	ChatWidget.h
	chat.qrc
	)
