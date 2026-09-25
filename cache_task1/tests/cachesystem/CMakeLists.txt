add_executable(test_cache_system test_cache_system.cpp)

target_link_libraries(test_cache_system
    PRIVATE
        cachesystem
        GTest::gtest
        GTest::gtest_main
)

target_compile_features(test_cache_system PRIVATE cxx_std_17)

gtest_discover_tests(test_cache_system)
