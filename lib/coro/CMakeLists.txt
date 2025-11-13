cmake_minimum_required(VERSION 3.10)
project(libedgejobcoro VERSION 0.0.1 LANGUAGES C ASM)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_language(ASM)

# Build options
#option(EDGE_COROUTINE_BUILD_SHARED "Build shared library" OFF)
#option(EDGE_COROUTINE_BUILD_STATIC "Build static library" ON)
option(EDGE_CORO_BUILD_EXAMPLES "Build examples" ON)

# Library source files
set(EDGE_COROUTINE_SOURCES
    "src/edge_coro.c"
    "src/edge_scheduler.c"
)

set(EDGE_COROUTINE_HEADERS
    "include/edge_coro.h"
    "include/edge_scheduler.h"
    "src/edge_coro_internal.h"
)

# Determine architecture and select appropriate assembly file
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(EDGE_CORO_ARCH "x86_64")
    if(WIN32)
        set(EDGE_COROUTINE_SOURCES ${EDGE_COROUTINE_SOURCES} "src/edge_coro_asm_x64_win.S")
    else()
        set(EDGE_COROUTINE_SOURCES ${EDGE_COROUTINE_SOURCES} "src/edge_coro_asm_x64_sysv.S")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(EDGE_CORO_ARCH "aarch64")
    set(EDGE_COROUTINE_SOURCES ${EDGE_COROUTINE_SOURCES} "src/edge_coro_asm_aarch64.S")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_library(edge_coro STATIC ${EDGE_COROUTINE_SOURCES})
target_include_directories(edge_coro
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(edge_coro PRIVATE ${ARCH_DEFINE})

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(edge_coro PRIVATE -m64)
endif()

target_link_libraries(edge_coro 
    PRIVATE 
    edge_base
)

set_target_properties(edge_coro PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${EDGE_COROUTINE_HEADERS}"
)

# Examples
if(EDGE_CORO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS edge_coro
    EXPORT edgeCoroTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT edgeCoroTargets
    FILE edgeCoroTargets.cmake
    NAMESPACE edge::coro::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_coro
)

# Generate config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edgeCoroConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edgeCoroConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_coro
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edgeCoroConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edgeCoroConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edgeCoroConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_coro
)

# Pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edge_coro.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/edge_coro.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edge_coro.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "libedgecoro Configuration Summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build examples:   ${EDGE_CORO_BUILD_EXAMPLES}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")