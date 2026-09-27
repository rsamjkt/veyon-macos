include(BuildVeyonPlugin)

# Receives the roles of teachers/admins (keys with limited rights) from the
# main admin, see core/src/AdminRoles.h
build_veyon_plugin(adminroles
	NAME AdminRoles
	SOURCES
	AdminRolesFeaturePlugin.cpp
	AdminRolesFeaturePlugin.h
	)
