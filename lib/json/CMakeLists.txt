cmake_minimum_required(VERSION 3.12)

project(libedgejson
    VERSION 1.0.0
    DESCRIPTION "Foundation json C library of edge engine"
    LANGUAGES C
)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(EDGE_JSON_BUILD_SHARED "Build shared library" OFF)
option(EDGE_JSON_BUILD_STATIC "Build static library" ON)
option(EDGE_JSON_BUILD_EXAMPLES "Build examples" OFF)
option(EDGE_JSON_BUILD_TESTS "Build tests" OFF)

# Library sources
set(EDGE_JSON_SOURCES
    "src/edge_json.c"
    "src/edge_json_parser.c"
    "src/edge_json_serializer.c"
)

set(EDGE_JSON_HEADERS
    "include/edge_json.h"
)

# Shared library
if(EDGE_JSON_BUILD_SHARED)
    add_library(edge_json SHARED ${EDGE_JSON_SOURCES})
    target_include_directories(edge_json
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    
    set_target_properties(edge_json PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${EDGE_JSON_HEADERS}"
    )
    
    target_link_libraries(edge_json PRIVATE edge_base)
endif()

# Static library
if(EDGE_JSON_BUILD_STATIC)
    add_library(edge_json_static STATIC ${EDGE_JSON_SOURCES})
    target_include_directories(edge_json_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    
    set_target_properties(edge_json_static PROPERTIES
        OUTPUT_NAME edge_json
        PUBLIC_HEADER "${EDGE_JSON_HEADERS}"
    )
    
    target_link_libraries(edge_json_static PRIVATE edge_base)
endif()

# Examples
if(EDGE_JSON_BUILD_EXAMPLES)
    #add_subdirectory(examples)
endif()

# Tests
if(EDGE_JSON_BUILD_TESTS)
    #enable_testing()
    #add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

if(EDGE_JSON_BUILD_SHARED)
    install(TARGETS edge_json
        EXPORT edgeJsonTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(EDGE_JSON_BUILD_STATIC)
    install(TARGETS edge_json_static
        EXPORT edgeJsonTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# Export targets
install(EXPORT edgeJsonTargets
    FILE edgeJsonTargets.cmake
    NAMESPACE edge::json::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_json
)

# Generate config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edgeJsonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edgeJsonConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_json
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edgeJsonConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edgeJsonConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edgeJsonConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_json
)

# Pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edge_json.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/edge_json.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edge_json.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "libedgejson Configuration Summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build shared:     ${EDGE_JSON_BUILD_SHARED}")
message(STATUS "  Build static:     ${EDGE_JSON_BUILD_STATIC}")
message(STATUS "  Build examples:   ${EDGE_JSON_BUILD_EXAMPLES}")
message(STATUS "  Build tests:      ${EDGE_JSON_BUILD_TESTS}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")