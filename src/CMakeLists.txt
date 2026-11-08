# Core logic without UI: parsers, provider APIs and storage. Shared with the tests.
add_library(iptvcore STATIC
    core/Types.h
    core/Types.cpp
    core/M3uParser.h
    core/M3uParser.cpp
    core/XtreamApi.h
    core/XtreamApi.cpp
    core/Database.h
    core/Database.cpp
    core/SourceStore.h
    core/SourceStore.cpp
    core/GzipDevice.h
    core/GzipDevice.cpp
    core/XmltvParser.h
    core/XmltvParser.cpp
    core/EpgStore.h
    core/EpgStore.cpp
    core/Catchup.h
    core/Catchup.cpp
    core/FreeTv.h
    core/FreeTv.cpp
    core/RadioBrowser.h
    core/RadioBrowser.cpp
    core/RadioStore.h
    core/RadioStore.cpp
    core/Parental.h
    core/Parental.cpp
    core/UpdateCheck.h
    core/UpdateCheck.cpp
)
target_include_directories(iptvcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(iptvcore PUBLIC Qt6::Core Qt6::Sql ZLIB::ZLIB)

qt_add_executable(iptvplayer
    main.cpp
    app/AppSettings.h
    app/AppSettings.cpp
    app/NetworkFactory.h
    app/NetworkFactory.cpp
    app/SourceManager.h
    app/EpgManager.h
    app/EpgManager.cpp
    app/VodManager.h
    app/VodManager.cpp
    app/SourceManager.cpp
    app/FreeTvCatalog.h
    app/FreeTvCatalog.cpp
    app/ParentalControl.h
    app/RadioManager.h
    app/RadioManager.cpp
    app/ParentalControl.cpp
    app/SpatialNavigation.h
    app/SpatialNavigation.cpp
    app/UpdateManager.h
    app/UpdateManager.cpp
    models/SourceListModel.h
    models/SourceListModel.cpp
    models/CategoryListModel.h
    models/CategoryListModel.cpp
    models/ChannelListModel.h
    models/ChannelListModel.cpp
    models/RadioStationModel.h
    models/RadioStationModel.cpp
    models/EpisodeListModel.h
    models/EpisodeListModel.cpp
    models/ContinueWatchingModel.h
    models/ContinueWatchingModel.cpp
    models/CategoryManagerModel.h
    models/CategoryManagerModel.cpp
    player/PlayerBackend.h
    player/QtMultimediaBackend.h
    player/QtMultimediaBackend.cpp
    player/PlayerController.h
    player/PlayerController.cpp
    platform/PictureInPicture.h
    platform/PictureInPicture.cpp
    platform/ScreenWakeLock.h
    platform/ScreenWakeLock.cpp
)

set(QML_SOURCES
    qml/Main.qml
    qml/AddSourcePage.qml
    qml/AppButton.qml
    qml/AppDialog.qml
    qml/AppScrollBar.qml
    qml/AppState.qml
    qml/AppSwitch.qml
    qml/AppTextField.qml
    qml/AppTheme.qml
    qml/ArtworkPlaceholder.qml
    qml/Avatar.qml
    qml/BottomNav.qml
    qml/CategoryChips.qml
    qml/CategoryMenu.qml
    qml/CategoryPanel.qml
    qml/ChannelCard.qml
    qml/ChannelDelegate.qml
    qml/Chip.qml
    qml/ContentRow.qml
    qml/DetailsBackdrop.qml
    qml/EmptyState.qml
    qml/EpisodeCard.qml
    qml/FocusRing.qml
    qml/GuidePage.qml
    qml/HomePage.qml
    qml/IconButton.qml
    qml/LivePage.qml
    qml/ManageCategoriesPage.qml
    qml/MiniPlayer.qml
    qml/MetaPill.qml
    qml/MovieDetailsPage.qml
    qml/NavRail.qml
    qml/Osd.qml
    qml/PageHeader.qml
    qml/PinDialog.qml
    qml/PlayerControls.qml
    qml/PlayerPage.qml
    qml/PosterCard.qml
    qml/RadioPage.qml
    qml/ProviderSwitcher.qml
    qml/SearchField.qml
    qml/SearchPage.qml
    qml/SeriesDetailsPage.qml
    qml/SettingsPage.qml
    qml/Shadow.qml
    qml/SleepTimer.qml
    qml/SleepTimerButton.qml
    qml/SortButton.qml
    qml/StationCard.qml
    qml/StatusBanner.qml
    qml/UpdateDialog.qml
    qml/VodPage.qml
)
set(QML_RESOURCES
    qml/icons/add.svg
    qml/icons/back.svg
    qml/icons/check.svg
    qml/icons/chevron-down.svg
    qml/icons/chevron-right.svg
    qml/icons/clock.svg
    qml/icons/close.svg
    qml/icons/delete.svg
    qml/icons/eye-off.svg
    qml/icons/film.svg
    qml/icons/filter.svg
    qml/icons/folder.svg
    qml/icons/fullscreen-exit.svg
    qml/icons/fullscreen.svg
    qml/icons/grid.svg
    qml/icons/guide.svg
    qml/icons/heart-outline.svg
    qml/icons/heart.svg
    qml/icons/home.svg
    qml/icons/info.svg
    qml/icons/link.svg
    qml/icons/list.svg
    qml/icons/live.svg
    qml/icons/lock-open.svg
    qml/icons/lock.svg
    qml/icons/moon.svg
    qml/icons/more.svg
    qml/icons/mute.svg
    qml/icons/next.svg
    qml/icons/pause.svg
    qml/icons/person.svg
    qml/icons/pip.svg
    qml/icons/play.svg
    qml/icons/previous.svg
    qml/icons/radio.svg
    qml/icons/retry.svg
    qml/icons/search.svg
    qml/icons/series.svg
    qml/icons/settings.svg
    qml/icons/sort.svg
    qml/icons/star-outline.svg
    qml/icons/star.svg
    qml/icons/subtitles.svg
    qml/icons/tv.svg
    qml/icons/volume.svg
    qml/fonts/Inter-Bold.ttf
    qml/fonts/Inter-Medium.ttf
    qml/fonts/Inter-Regular.ttf
    qml/fonts/Inter-SemiBold.ttf
    qml/images/shadow.png
)

# Serve the files from the module root (next to its qmldir) instead of a qml/
# sub-folder. Otherwise QML's implicit directory import shadows module types
# and singletons such as AppTheme stop working.
foreach(file IN LISTS QML_SOURCES QML_RESOURCES)
    string(REGEX REPLACE "^qml/" "" alias "${file}")
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${alias})
endforeach()
set_source_files_properties(qml/AppTheme.qml qml/AppState.qml qml/SleepTimer.qml
                            PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# qmlcachegen 6.12.0 crashes on this file (Qt bug, seen on Arch Linux). It is
# compiled when the app starts instead; all other files are still precompiled.
set_source_files_properties(qml/HomePage.qml PROPERTIES QT_QML_SKIP_CACHEGEN TRUE)

# Ahead-of-time compilation of QML. Turn off (-DIPTV_QML_AOT=OFF) if your Qt's
# qmlcachegen crashes; QML is then compiled when the app starts instead.
option(IPTV_QML_AOT "Compile QML ahead of time with qmlcachegen" ON)
set(QML_CACHEGEN_OPTION)
if(NOT IPTV_QML_AOT)
    set(QML_CACHEGEN_OPTION NO_CACHEGEN)
endif()

qt_add_qml_module(iptvplayer
    URI IPTVPlayer
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
    RESOURCES ${QML_RESOURCES}
    ${QML_CACHEGEN_OPTION}
)

# App icon for the window (fixed resource path, the same in every Qt version).
qt_add_resources(iptvplayer "appicon"
    PREFIX "/appicon"
    BASE ${CMAKE_SOURCE_DIR}/assets/icon
    FILES ${CMAKE_SOURCE_DIR}/assets/icon/app-icon-256.png)

# Windows: icon of IPTVPlayer.exe and its version details.
if(WIN32)
    set(ICON_PATH "${CMAKE_SOURCE_DIR}/assets/icon/app.ico")
    configure_file(${CMAKE_SOURCE_DIR}/assets/windows/iptvplayer.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/iptvplayer.rc @ONLY)
    target_sources(iptvplayer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/iptvplayer.rc)
endif()

target_include_directories(iptvplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/player
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/models)

target_compile_definitions(iptvplayer PRIVATE
    APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(iptvplayer PRIVATE
    iptvcore
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Svg)

# Android only installs a new APK over the old one if this number is higher.
# CI passes the build number.
set(IPTV_ANDROID_VERSION_CODE 1 CACHE STRING "Android versionCode (must grow with every release)")

set_target_properties(iptvplayer PROPERTIES
    OUTPUT_NAME "IPTVPlayer"
    # Keep the binary away from the IPTVPlayer/ QML module folder (same name).
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
    QT_ANDROID_VERSION_CODE ${IPTV_ANDROID_VERSION_CODE}
    # Manifest with icon and launch screen, launcher icons (android/)
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/android
    QT_ANDROID_PACKAGE_NAME io.github.sadikbr.iptvplayer
    QT_ANDROID_APP_NAME "IPTV Player")

# Android is packaged by the "apk" target instead.
if(NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS iptvplayer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(UNIX AND NOT APPLE)
        # Start menu entry and icons (Linux)
        install(FILES ${CMAKE_SOURCE_DIR}/assets/io.github.sadikbr.iptvplayer.desktop
                DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/icon/hicolor
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
    endif()
endif()

# Smoke test: start the app off-screen, load all QML and fail on any QML warning.
if(NOT ANDROID)
    add_test(NAME qml_smoke COMMAND iptvplayer --smoke-test)
    set_tests_properties(qml_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;IPTV_SMOKE_PLAYLIST=${CMAKE_SOURCE_DIR}/tests/fixtures/playlist.m3u"
        TIMEOUT 60)
endif()
