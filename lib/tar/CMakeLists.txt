cmake_minimum_required(VERSION 3.10)
project(libedgetar VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(EDGE_TAR_BUILD_SHARED "Build shared library" OFF)
option(EDGE_TAR_BUILD_STATIC "Build static library" ON)
option(EDGE_TAR_BUILD_EXAMPLES "Build examples" ON)

# Library source files
set(EDGE_TAR_SOURCES
    "src/edge_tar.c"
    "src/edge_tar_read.c"
    "src/edge_tar_write.c"
    "src/edge_tar_utils.c"
)

set(EDGE_TAR_HEADERS
    "include/edge_tar.h"
    "src/edge_tar_internal.h"
)

# Shared library
if(EDGE_TAR_BUILD_SHARED)
    add_library(edge_tar SHARED ${EDGE_TAR_SOURCES})
    target_include_directories(edge_tar
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(edge_tar PRIVATE edge_base)

    set_target_properties(edge_tar PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${EDGE_TAR_HEADERS}"
    )

    # Platform-specific settings
    if(UNIX AND NOT APPLE)
        target_compile_definitions(edge_tar PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    
    # Compiler warnings
    if(MSVC)
        target_compile_options(edge_tar PRIVATE /W4)
    else()
        target_compile_options(edge_tar PRIVATE -Wall -pedantic)
    endif()

    target_compile_definitions(edge_tar PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Static library
if(EDGE_TAR_BUILD_STATIC)
    add_library(edge_tar_static STATIC ${EDGE_TAR_SOURCES})
    target_include_directories(edge_tar_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(edge_tar_static PRIVATE edge_base)
    
    set_target_properties(edge_tar_static PROPERTIES
        OUTPUT_NAME edge_tar
        PUBLIC_HEADER "${EDGE_ZIP_HEADERS}"
    )

    # Platform-specific settings
    if(UNIX AND NOT APPLE)
        target_compile_definitions(edge_tar_static PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    
    # Compiler warnings
    if(MSVC)
        target_compile_options(edge_tar_static PRIVATE /W4)
        target_compile_definitions(edge_tar_static PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(edge_tar_static PRIVATE -Wall -pedantic)
    endif()
endif()

# Examples
if(EDGE_TAR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

if(EDGE_TAR_BUILD_SHARED)
    install(TARGETS edge_tar
        EXPORT edgeTarTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(EDGE_TAR_BUILD_STATIC)
    install(TARGETS edge_tar_static
        EXPORT edgeTarTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# Export targets
install(EXPORT edgeTarTargets
    FILE edgeTarTargets.cmake
    NAMESPACE edge::tar::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_tar
)

# Generate config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edgeTarConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edgeTarConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_tar
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edgeTarConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edgeTarConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edgeTarConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_tar
)

# Pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edge_tar.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/edge_tar.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edge_tar.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "libedgetar Configuration Summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build shared:     ${EDGE_TAR_BUILD_SHARED}")
message(STATUS "  Build static:     ${EDGE_TAR_BUILD_STATIC}")
message(STATUS "  Build examples:   ${EDGE_TAR_BUILD_EXAMPLES}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")