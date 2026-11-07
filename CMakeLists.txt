cmake_minimum_required(VERSION 3.21)

project(IPTVPlayer
    VERSION 0.3.0
    DESCRIPTION "Cross-platform IPTV player"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CI builds with the latest Qt 6.10. 6.4 is the oldest version the code builds with.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Sql Concurrent Quick QuickControls2 Multimedia Svg)

qt_standard_project_setup()
if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()

# zlib (for .xml.gz TV guides). Linux and Android have it; elsewhere (Windows)
# it is downloaded and built with the app.
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    message(STATUS "zlib not found on the system, building it from source")
    include(FetchContent)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(zlib)
    target_include_directories(zlibstatic INTERFACE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

enable_testing()

add_subdirectory(src)

option(IPTV_BUILD_TESTS "Build unit tests" ON)
if(IPTV_BUILD_TESTS AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
