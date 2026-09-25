# CMakeLists.txt (корневой)
cmake_minimum_required(VERSION 3.14)
project(CacheSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(cachesystem INTERFACE)

target_include_directories(cachesystem INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# опционально: чтобы IDE видела заголовки как часть таргета
target_sources(cachesystem INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
        include/cache/arc_cache.hpp
        include/cache/lfu_cache.hpp
        include/cache/lirs_cache.hpp
        include/cache/optimal_cache.hpp
        include/cache/two_q_cache.hpp
        include/cachesystem/cache_system.hpp
        include/cachesystem/cache_wrapper.hpp
)

enable_testing()
add_subdirectory(tests)
