# argh.h as a CMake package. The library is one header: the target only
# carries its include path and the C99 requirement. As before, define
# ARGH_IMPLEMENTATION in one source file of your program.
#
#   FetchContent_Declare(argh GIT_REPOSITORY https://github.com/ilyabrin/argh GIT_TAG v1.8.0)
#   FetchContent_MakeAvailable(argh)          # or add_subdirectory(argh)
#   find_package(argh 1.8 REQUIRED)           # after cmake --install
#   target_link_libraries(app PRIVATE argh::argh)
cmake_minimum_required(VERSION 3.14)

# The version lives in argh.h only
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/argh.h" _argh_version_lines REGEX "^#define ARGH_VERSION_(MAJOR|MINOR|PATCH) ")
foreach(_line IN LISTS _argh_version_lines)
    if(_line MATCHES "^#define ARGH_VERSION_(MAJOR|MINOR|PATCH) +([0-9]+)")
        set(_argh_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    endif()
endforeach()

project(argh
    VERSION "${_argh_MAJOR}.${_argh_MINOR}.${_argh_PATCH}"
    DESCRIPTION "Single-header command-line argument parser for C99"
    HOMEPAGE_URL "https://github.com/ilyabrin/argh"
    LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(_argh_top_level ON)
else()
    set(_argh_top_level OFF)
endif()
option(ARGH_INSTALL "Generate the install target" ${_argh_top_level})
option(ARGH_BUILD_TESTS "Build the tests (ctest)" ${_argh_top_level})

add_library(argh INTERFACE)
add_library(argh::argh ALIAS argh)
target_compile_features(argh INTERFACE c_std_99)

include(GNUInstallDirs)
target_include_directories(argh INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(ARGH_INSTALL)
    include(CMakePackageConfigHelpers)
    set(_argh_cmake_dir "${CMAKE_INSTALL_DATADIR}/cmake/argh")

    install(FILES argh.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(TARGETS argh EXPORT arghTargets)
    install(EXPORT arghTargets NAMESPACE argh:: DESTINATION "${_argh_cmake_dir}")

    configure_package_config_file(cmake/arghConfig.cmake.in
        "${PROJECT_BINARY_DIR}/arghConfig.cmake"
        INSTALL_DESTINATION "${_argh_cmake_dir}")
    # 1.x accepts any later 1.x; a header has no architecture
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/arghConfigVersion.cmake"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)
    install(FILES
        "${PROJECT_BINARY_DIR}/arghConfig.cmake"
        "${PROJECT_BINARY_DIR}/arghConfigVersion.cmake"
        DESTINATION "${_argh_cmake_dir}")

    # pkg-config: cc $(pkg-config --cflags argh)
    configure_file(cmake/argh.pc.in "${PROJECT_BINARY_DIR}/argh.pc" @ONLY)
    install(FILES "${PROJECT_BINARY_DIR}/argh.pc" DESTINATION "${CMAKE_INSTALL_DATADIR}/pkgconfig")
endif()

if(ARGH_BUILD_TESTS)
    enable_testing()
    add_executable(argh_test tests/test_argh.c)
    target_link_libraries(argh_test PRIVATE argh::argh)
    add_test(NAME unit COMMAND argh_test)

    add_executable(argh_nostdio tests/nostdio_check.c)
    target_link_libraries(argh_nostdio PRIVATE argh::argh)
    add_test(NAME nostdio COMMAND argh_nostdio)

    add_executable(argh_static tests/static_check.c tests/static_check_other.c)
    target_link_libraries(argh_static PRIVATE argh::argh)
    add_test(NAME static COMMAND argh_static -v)
endif()
