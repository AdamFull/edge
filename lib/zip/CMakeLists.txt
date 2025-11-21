cmake_minimum_required(VERSION 3.12)

project(libedgezip
    VERSION 1.0.0
    DESCRIPTION "Foundation zip C library of edge engine"
    LANGUAGES C
)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(EDGE_ZIP_BUILD_SHARED "Build shared library" OFF)
option(EDGE_ZIP_BUILD_STATIC "Build static library" ON)
option(EDGE_ZIP_BUILD_EXAMPLES "Build examples" ON)

# Library sources
set(EDGE_ZIP_SOURCES
    "src/edge_zip.c"
    "src/edge_zip_read.c"
    "src/edge_zip_utils.c"
    "src/edge_zip_write.c"
)

set(EDGE_ZIP_HEADERS
    "include/edge_zip.h"
    "include/edge_zip_internal.h"
)

# Shared library
if(EDGE_ZIP_BUILD_SHARED)
    add_library(edge_zip SHARED ${EDGE_ZIP_SOURCES})
    target_include_directories(edge_zip
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    
    target_link_libraries(edge_zip PRIVATE edge_base)

    set_target_properties(edge_zip PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${EDGE_ZIP_HEADERS}"
    )
    target_compile_definitions(edge_zip PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Static library
if(EDGE_ZIP_BUILD_STATIC)
    add_library(edge_zip_static STATIC ${EDGE_ZIP_SOURCES})
    target_include_directories(edge_zip_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(edge_zip_static PRIVATE edge_base)
    
    set_target_properties(edge_zip_static PROPERTIES
        OUTPUT_NAME edge_zip
        PUBLIC_HEADER "${EDGE_ZIP_HEADERS}"
    )
    target_compile_definitions(edge_zip_static PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Examples
if(EDGE_ZIP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

if(EDGE_ZIP_BUILD_SHARED)
    install(TARGETS edge_zip
        EXPORT edgeZipTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(EDGE_ZIP_BUILD_STATIC)
    install(TARGETS edge_zip_static
        EXPORT edgeZipTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# Export targets
install(EXPORT edgeZipTargets
    FILE edgeZipTargets.cmake
    NAMESPACE edge::zip::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_zip
)

# Generate config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edgeZipConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edgeZipConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_zip
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edgeZipConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edgeZipConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edgeZipConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_zip
)

# Pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edge_zip.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/edge_zip.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edge_zip.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "libedgezip Configuration Summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build shared:     ${EDGE_ZIP_BUILD_SHARED}")
message(STATUS "  Build static:     ${EDGE_ZIP_BUILD_STATIC}")
message(STATUS "  Build examples:   ${EDGE_ZIP_BUILD_EXAMPLES}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")