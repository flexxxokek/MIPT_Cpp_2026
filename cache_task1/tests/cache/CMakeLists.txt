# Список тестовых файлов для кэшей
set(CACHE_TEST_SOURCES
    test_arc_cache.cpp
    test_lfu_cache.cpp
    test_lirs_cache.cpp
    test_optimal_cache.cpp
    test_two_q_cache.cpp
)

foreach(test_src ${CACHE_TEST_SOURCES})
    # Имя таргета из имени файла (test_arc_cache.cpp -> test_arc_cache)
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name} ${test_src})

    target_link_libraries(${test_name}
        PRIVATE
            cachesystem
            GTest::gtest
            GTest::gtest_main
    )

    target_compile_features(${test_name} PRIVATE cxx_std_17)

    gtest_discover_tests(${test_name})
endforeach()
