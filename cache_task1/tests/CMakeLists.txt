# Общие настройки для всех тестов
enable_testing()

# Находим GTest (если ещё не подключен в корне проекта)
find_package(GTest REQUIRED)
include(GoogleTest)

# Общая interface-библиотека с include-путями проекта,
# чтобы не дублировать target_include_directories в каждом подкаталоге
add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

add_subdirectory(cache)
add_subdirectory(cachesystem)
