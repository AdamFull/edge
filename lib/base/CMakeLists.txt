cmake_minimum_required(VERSION 3.10)
project(libedgebase VERSION 1.0.0 LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(EDGE_BASE_BUILD_EXAMPLES "Build examples" ON)

set(EDGE_BASE_SOURCES
    "src/arena.cpp"
    "src/hash.cpp"
    "src/random.cpp"
    "src/threads.cpp"
    "src/vmem.cpp"
)

set(EDGE_BASE_HEADERS
    "include/allocator.hpp"
    "include/arena.hpp"
    "include/array.hpp"
    "include/bitarray.hpp"
    "include/hash.hpp"
    "include/hashmap.hpp"
    "include/list.hpp"
    "include/math.hpp"
    "include/mpmc_queue.hpp"
    "include/platform_detect.hpp"
    "include/random.hpp"
    "include/stddef.hpp"
    "include/string.hpp"
    "include/vmem.hpp"
)

# Library
add_library(edge_base STATIC
    ${EDGE_BASE_SOURCES}
)

# Include directories
target_include_directories(edge_base PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(edge_base PRIVATE _CRT_SECURE_NO_WARNINGS)

# Compiler flags
if(MSVC)
    target_compile_options(edge_base PRIVATE /W4)
else()
    target_compile_options(edge_base PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(edge_base PRIVATE Threads::Threads m)
    target_compile_definitions(edge_base PRIVATE _GNU_SOURCE)
endif()

if(EDGE_BASE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS edge_base
    EXPORT edgeBaseTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${EDGE_BASE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT edgeBaseTargets
    FILE edgeBaseTargets.cmake
    NAMESPACE edge::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_base
)