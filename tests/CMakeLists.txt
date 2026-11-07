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
