# Uses aargh the way a CMake project would: ARGH_VIA is find_package,
# fetchcontent or subdirectory; ARGH_SOURCE is this repository.
# ARGH_NAME=argh uses the deprecated v1.8 names instead.
cmake_minimum_required(VERSION 3.14)
project(argh_package_test LANGUAGES C)

if(NOT ARGH_NAME)
    set(ARGH_NAME aargh)
endif()

if(ARGH_VIA STREQUAL "find_package")
    find_package(${ARGH_NAME} 1.0 REQUIRED)
elseif(ARGH_VIA STREQUAL "fetchcontent")
    include(FetchContent)
    FetchContent_Declare(${ARGH_NAME} SOURCE_DIR "${ARGH_SOURCE}")
    FetchContent_MakeAvailable(${ARGH_NAME})
elseif(ARGH_VIA STREQUAL "subdirectory")
    add_subdirectory("${ARGH_SOURCE}" aargh)
else()
    message(FATAL_ERROR "Set ARGH_VIA to find_package, fetchcontent or subdirectory")
endif()

# As a dependency, aargh adds no tests and no install rules of its own
if(TARGET argh_test)
    message(FATAL_ERROR "aargh built its tests as a dependency")
endif()

add_executable(app app.c)
target_link_libraries(app PRIVATE ${ARGH_NAME}::${ARGH_NAME})
