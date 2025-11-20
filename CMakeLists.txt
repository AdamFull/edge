cmake_minimum_required(VERSION 3.22.1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EDGE_USE_TSAN)
    if(EDGE_USE_ASAN)
        add_compile_definitions(ASAN_ENABLED=1)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()

    if(EDGE_USE_TSAN)
        add_compile_definitions(TSAN_ENABLED=1)
        add_compile_options(-fsanitize=thread -g)
        add_link_options(-fsanitize=thread -ltsan)
    endif()
endif()

add_subdirectory(external)
add_subdirectory(lib)
add_subdirectory(engine)