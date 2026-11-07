qt_add_executable(iptvplayer
    main.cpp
    app/AppSettings.h
    app/AppSettings.cpp
    player/PlayerBackend.h
    player/QtMultimediaBackend.h
    player/QtMultimediaBackend.cpp
    player/PlayerController.h
    player/PlayerController.cpp
    platform/ScreenWakeLock.h
    platform/ScreenWakeLock.cpp
)

set(QML_SOURCES
    qml/Main.qml
    qml/AppTheme.qml
    qml/HomePage.qml
    qml/PlayerPage.qml
    qml/PlayerControls.qml
    qml/IconButton.qml
)
set(QML_RESOURCES
    qml/icons/back.svg
    qml/icons/close.svg
    qml/icons/fullscreen.svg
    qml/icons/fullscreen-exit.svg
    qml/icons/info.svg
    qml/icons/mute.svg
    qml/icons/pause.svg
    qml/icons/play.svg
    qml/icons/retry.svg
    qml/icons/tv.svg
    qml/icons/volume.svg
)

# Serve the files from the module root (next to its qmldir) instead of a qml/
# sub-folder. Otherwise QML's implicit directory import shadows module types
# and singletons such as AppTheme stop working.
foreach(file IN LISTS QML_SOURCES QML_RESOURCES)
    string(REGEX REPLACE "^qml/" "" alias "${file}")
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias})
endforeach()
set_source_files_properties(qml/AppTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(iptvplayer
    URI IPTVPlayer
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
    RESOURCES ${QML_RESOURCES}
)

target_include_directories(iptvplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/player)

target_compile_definitions(iptvplayer PRIVATE
    APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(iptvplayer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Svg)

set_target_properties(iptvplayer PROPERTIES
    OUTPUT_NAME "IPTVPlayer"
    # Keep the binary away from the IPTVPlayer/ QML module folder (same name).
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
    QT_ANDROID_VERSION_CODE 1
    QT_ANDROID_PACKAGE_NAME io.github.sadikbr.iptvplayer
    QT_ANDROID_APP_NAME "IPTV Player")

# Android is packaged by the "apk" target instead.
if(NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS iptvplayer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Smoke test: start the app off-screen, load all QML and fail on any QML warning.
if(NOT ANDROID)
    add_test(NAME qml_smoke COMMAND iptvplayer --smoke-test)
    set_tests_properties(qml_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30)
endif()
