include(BuildVeyonPlugin)

# One-click exam mode: internet only for the exam sites, forbidden apps closed,
# system keyboard shortcuts locked - all undone with one click
build_veyon_plugin(exammode
	NAME ExamMode
	SOURCES
	ExamModeFeaturePlugin.cpp
	ExamModeFeaturePlugin.h
	ExamModeDialog.cpp
	ExamModeDialog.h
	exammode.qrc
	)
