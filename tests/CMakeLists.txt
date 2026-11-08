function(iptv_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE iptvcore Qt6::Test Qt6::Concurrent)
    target_compile_definitions(${name} PRIVATE
        FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

iptv_add_test(tst_m3uparser)
iptv_add_test(tst_xtreamapi)
iptv_add_test(tst_sourcestore)
iptv_add_test(tst_xmltv)
iptv_add_test(tst_epgstore)
iptv_add_test(tst_updatecheck)
iptv_add_test(tst_catchup)
iptv_add_test(tst_parental)
iptv_add_test(tst_freetv)

# The player logic (reconnect etc.) with a fake backend.
iptv_add_test(tst_playercontroller)
target_sources(tst_playercontroller PRIVATE
    ${CMAKE_SOURCE_DIR}/src/player/PlayerBackend.h
    ${CMAKE_SOURCE_DIR}/src/player/PlayerController.h
    ${CMAKE_SOURCE_DIR}/src/player/PlayerController.cpp
    ${CMAKE_SOURCE_DIR}/src/player/QtMultimediaBackend.h
    ${CMAKE_SOURCE_DIR}/src/player/QtMultimediaBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/ScreenWakeLock.cpp)
target_include_directories(tst_playercontroller PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_playercontroller PRIVATE Qt6::Gui Qt6::Qml Qt6::Multimedia)

# A real stream that drops, played by Qt Multimedia (opt-in: IPTV_STREAM_TEST=1).
iptv_add_test(tst_streamreconnect)
target_sources(tst_streamreconnect PRIVATE
    ${CMAKE_SOURCE_DIR}/src/player/PlayerBackend.h
    ${CMAKE_SOURCE_DIR}/src/player/PlayerController.h
    ${CMAKE_SOURCE_DIR}/src/player/PlayerController.cpp
    ${CMAKE_SOURCE_DIR}/src/player/QtMultimediaBackend.h
    ${CMAKE_SOURCE_DIR}/src/player/QtMultimediaBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/ScreenWakeLock.cpp)
target_include_directories(tst_streamreconnect PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(tst_streamreconnect PRIVATE
    STREAM_SERVER="${CMAKE_CURRENT_SOURCE_DIR}/stream_server.py")
target_link_libraries(tst_streamreconnect PRIVATE Qt6::Gui Qt6::Qml Qt6::Multimedia)
set_tests_properties(tst_streamreconnect PROPERTIES TIMEOUT 180)
