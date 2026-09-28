include(BuildVeyonPlugin)

# "Lab clean mode": the files of the student accounts are moved away at every
# restart (kept for some days), like a light-weight "Deep Freeze"
build_veyon_plugin(labclean
	NAME LabClean
	SOURCES
	LabCleanFeaturePlugin.cpp
	LabCleanFeaturePlugin.h
	labclean.qrc
	)
