cmake_minimum_required(VERSION 3.16)

# The version of a release: shown in the watch's log, and in the web installer.
if(NOT DEFINED PROJECT_VER)
    set(PROJECT_VER "0.1.0")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(knoedel_watch)
