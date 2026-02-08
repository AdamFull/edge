cmake_minimum_required(VERSION 3.10)
project(libedgebase VERSION 1.0.0 LANGUAGES C CXX ASM)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(EDGE_BASE_BUILD_EXAMPLES "Build examples" ON)

set(EDGE_BASE_SOURCES
        "src/arena.cpp"
        "src/fiber.cpp"
        "src/filesystem.cpp"
        "src/hash.cpp"
        "src/random.cpp"
        "src/scheduler.cpp"
        "src/threads.cpp"
        "src/uuid.cpp"
        "src/vmem.cpp"
)

set(EDGE_BASE_HEADERS
        "include/allocator.hpp"
        "include/arena.hpp"
        "include/array.hpp"
        "include/bitarray.hpp"
        "include/callable.hpp"
        "include/fiber.hpp"
        "include/filesystem.hpp"
        "include/free_index_list.hpp"
        "include/handle_pool.hpp"
        "include/hash.hpp"
        "include/hashmap.hpp"
        "include/list.hpp"
        "include/math.hpp"
        "include/mpmc_queue.hpp"
        "include/platform_detect.hpp"
        "include/random.hpp"
        "include/random_access_iterator.hpp"
        "include/scheduler.hpp"
        "include/span.hpp"
        "include/stddef.hpp"
        "include/string.hpp"
        "include/string_view.hpp"
        "include/threads.hpp"
        "include/uuid.hpp"
        "include/vmem.hpp"
)

set(EDGE_BASE_SOURCES_WIN
        "src/filesystem_win.cpp"
)

set(EDGE_BASE_SOURCES_UNIX
        "src/filesystem_unix.cpp"
)

# Determine architecture and select appropriate assembly file
if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(EDGE_CORO_ARCH "x86_64")
    if (WIN32)
        set(EDGE_BASE_SOURCES ${EDGE_BASE_SOURCES} "src/fiber_asm_x64_win.asm")
        set_source_files_properties("src/fiber_asm_x64_win.asm" PROPERTIES LANGUAGE ASM)
    else ()
        set(EDGE_BASE_SOURCES ${EDGE_BASE_SOURCES} "src/fiber_asm_x64_sysv.asm")
    endif ()
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(EDGE_CORO_ARCH "aarch64")
    set(EDGE_BASE_SOURCES ${EDGE_BASE_SOURCES} "src/fiber_asm_aarch64.asm")
else ()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

#OS dependent sources
if(WIN32)
    list(APPEND EDGE_BASE_SOURCES ${EDGE_BASE_SOURCES_WIN})
elseif(UNIX)
    list(APPEND EDGE_BASE_SOURCES ${EDGE_BASE_SOURCES_UNIX})
endif()

# Library
add_library(edge_base STATIC
        ${EDGE_BASE_SOURCES}
)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(edge_base PRIVATE -m64)
endif ()

# Include directories
target_include_directories(edge_base PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(edge_base PRIVATE _CRT_SECURE_NO_WARNINGS)

# Compiler flags
if (MSVC)
    target_compile_options(edge_base PRIVATE /W4)
else ()
    target_compile_options(edge_base PRIVATE -Wall -Wextra -Wpedantic)
endif ()

if (UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(edge_base PRIVATE Threads::Threads m)
    target_compile_definitions(edge_base PRIVATE _GNU_SOURCE)
endif ()

if (EDGE_BASE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

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