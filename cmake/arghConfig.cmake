# find_package(argh) and argh::argh: the v1.8 names, deprecated until 2.0.
# Use find_package(aargh) and aargh::aargh instead.
include(CMakeFindDependencyMacro)
find_dependency(aargh "${argh_FIND_VERSION}" CONFIG
    PATHS "${CMAKE_CURRENT_LIST_DIR}/../aargh" NO_DEFAULT_PATH)
if(NOT TARGET argh::argh)
    add_library(argh::argh INTERFACE IMPORTED)
    set_target_properties(argh::argh PROPERTIES INTERFACE_LINK_LIBRARIES aargh::aargh)
endif()
