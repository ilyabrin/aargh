# aargh (argh.h) as a CMake package. The library is one header: the target
# only carries its include path and the C99 requirement. As before, define
# ARGH_IMPLEMENTATION in one source file of your program.
#
#   FetchContent_Declare(aargh GIT_REPOSITORY https://github.com/ilyabrin/aargh GIT_TAG v1.11.0)
#   FetchContent_MakeAvailable(aargh)         # or add_subdirectory(aargh)
#   find_package(aargh 1.11 REQUIRED)          # after cmake --install
#   target_link_libraries(app PRIVATE aargh::aargh)
#
# The header installs to include/aargh/argh.h, with include/aargh on the
# include path: your code keeps #include "argh.h", and it can't collide with
# another argh.h on the system.
cmake_minimum_required(VERSION 3.14)

# The version lives in argh.h only
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/argh.h" _argh_version_lines REGEX "^#define ARGH_VERSION_(MAJOR|MINOR|PATCH) ")
foreach(_line IN LISTS _argh_version_lines)
    if(_line MATCHES "^#define ARGH_VERSION_(MAJOR|MINOR|PATCH) +([0-9]+)")
        set(_argh_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endif()
endforeach()

project(aargh
    VERSION "${_argh_MAJOR}.${_argh_MINOR}.${_argh_PATCH}"
    DESCRIPTION "Single-header command-line argument parser for C99"
    HOMEPAGE_URL "https://github.com/ilyabrin/aargh"
    LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_argh_top_level ON)
else()
    set(_argh_top_level OFF)
endif()
option(ARGH_INSTALL "Generate the install target" ${_argh_top_level})
option(ARGH_BUILD_TESTS "Build the tests (ctest)" ${_argh_top_level})
# v1.8 names, deprecated until 2.0. Package registries turn this off, since
# share/cmake/argh belongs to another library there.
option(ARGH_INSTALL_LEGACY_NAME "Also install find_package(argh) and argh::argh (deprecated)" ON)

add_library(aargh INTERFACE)
add_library(aargh::aargh ALIAS aargh)
# Deprecated v1.8 name, for FetchContent and add_subdirectory users
add_library(argh::argh ALIAS aargh)
target_compile_features(aargh INTERFACE c_std_99)

include(GNUInstallDirs)
target_include_directories(aargh INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/aargh>")

if(ARGH_INSTALL)
    include(CMakePackageConfigHelpers)
    set(_argh_cmake_dir "${CMAKE_INSTALL_DATADIR}/cmake/aargh")

    install(FILES argh.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/aargh")
    install(TARGETS aargh EXPORT aarghTargets)
    install(EXPORT aarghTargets NAMESPACE aargh:: DESTINATION "${_argh_cmake_dir}")

    configure_package_config_file(cmake/aarghConfig.cmake.in
        "${PROJECT_BINARY_DIR}/aarghConfig.cmake"
        INSTALL_DESTINATION "${_argh_cmake_dir}")
    # 1.x accepts any later 1.x; a header has no architecture
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/aarghConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)
    install(FILES
        "${PROJECT_BINARY_DIR}/aarghConfig.cmake"
        "${PROJECT_BINARY_DIR}/aarghConfigVersion.cmake"
        DESTINATION "${_argh_cmake_dir}")

    # pkg-config: cc $(pkg-config --cflags aargh)
    configure_file(cmake/aargh.pc.in "${PROJECT_BINARY_DIR}/aargh.pc" @ONLY)
    install(FILES "${PROJECT_BINARY_DIR}/aargh.pc" DESTINATION "${CMAKE_INSTALL_DATADIR}/pkgconfig")

    if(ARGH_INSTALL_LEGACY_NAME)
        install(FILES cmake/arghConfig.cmake DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/argh")
        install(FILES "${PROJECT_BINARY_DIR}/aarghConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/argh"
            RENAME arghConfigVersion.cmake)
    endif()
endif()

if(ARGH_BUILD_TESTS)
    enable_testing()
    add_executable(argh_test tests/test_argh.c)
    target_link_libraries(argh_test PRIVATE aargh::aargh)
    add_test(NAME unit COMMAND argh_test)

    add_executable(argh_nostdio tests/nostdio_check.c)
    target_link_libraries(argh_nostdio PRIVATE aargh::aargh)
    add_test(NAME nostdio COMMAND argh_nostdio)

    add_executable(argh_static tests/static_check.c tests/static_check_other.c)
    target_link_libraries(argh_static PRIVATE aargh::aargh)
    add_test(NAME static COMMAND argh_static -v)
endif()
