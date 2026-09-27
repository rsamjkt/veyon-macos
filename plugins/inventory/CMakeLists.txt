include(BuildVeyonPlugin)

# Hardware, disks and installed software of a computer, collected by the
# Aruni Gateway into the inventory
build_veyon_plugin(inventory
	NAME Inventory
	SOURCES
	InventoryFeaturePlugin.cpp
	InventoryFeaturePlugin.h
	)
