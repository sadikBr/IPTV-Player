cmake_minimum_required(VERSION 3.21)

project(IPTVPlayer
    VERSION 0.1.0
    DESCRIPTION "Cross-platform IPTV player"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.8 LTS is the target (used in CI). 6.4 is the oldest version the code builds with.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Quick QuickControls2 Multimedia Svg)

qt_standard_project_setup()
if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()

enable_testing()

add_subdirectory(src)
