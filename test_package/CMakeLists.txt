cmake_minimum_required(VERSION 3.15)
project(test_package LANGUAGES C)

find_package(aargh REQUIRED CONFIG)

add_executable(app ../tests/package/app.c)
target_link_libraries(app PRIVATE aargh::aargh)
