cmake_minimum_required(VERSION 3.21)

project(IPTVPlayer
    VERSION 0.2.0
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

enable_testing()

add_subdirectory(src)

option(IPTV_BUILD_TESTS "Build unit tests" ON)
if(IPTV_BUILD_TESTS AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
