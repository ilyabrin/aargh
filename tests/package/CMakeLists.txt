# Uses argh the way a CMake project would: ARGH_VIA is find_package,
# fetchcontent or subdirectory; ARGH_SOURCE is this repository.
cmake_minimum_required(VERSION 3.14)
project(argh_package_test LANGUAGES C)

if(ARGH_VIA STREQUAL "find_package")
    find_package(argh 1.0 REQUIRED)
elseif(ARGH_VIA STREQUAL "fetchcontent")
    include(FetchContent)
    FetchContent_Declare(argh SOURCE_DIR "${ARGH_SOURCE}")
    FetchContent_MakeAvailable(argh)
elseif(ARGH_VIA STREQUAL "subdirectory")
    add_subdirectory("${ARGH_SOURCE}" argh)
else()
    message(FATAL_ERROR "Set ARGH_VIA to find_package, fetchcontent or subdirectory")
endif()

# As a dependency, argh adds no tests and no install rules of its own
if(TARGET argh_test)
    message(FATAL_ERROR "argh built its tests as a dependency")
endif()

add_executable(app app.c)
target_link_libraries(app PRIVATE argh::argh)
