cmake_minimum_required(VERSION 3.12)

project(libedgejson
    VERSION 1.0.0
    DESCRIPTION "Foundation json CXX library of edge engine"
    LANGUAGES C CXX
)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(EDGE_JSON_BUILD_TESTS "Build tests" ON)

# Library sources
set(EDGE_JSON_SOURCES
    #"src/edge_json.c"
    #"src/edge_json_parser.c"
    #"src/edge_json_serializer.c"
    "src/json.cpp"
    "src/json_parser.cpp"
    "src/json_serializer.cpp"
)

set(EDGE_JSON_HEADERS
    #"include/edge_json.h"
    "include/json.hpp"
)

add_library(edge_json STATIC ${EDGE_JSON_SOURCES})
target_include_directories(edge_json
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(edge_json PROPERTIES
    OUTPUT_NAME edge_json
    PUBLIC_HEADER "${EDGE_JSON_HEADERS}"
)

target_link_libraries(edge_json PRIVATE edge_base)

# Tests
if(EDGE_JSON_BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS edge_json
    EXPORT edgeJsonTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

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
message(STATUS "  Build examples:   ${EDGE_JSON_BUILD_EXAMPLES}")
message(STATUS "  Build tests:      ${EDGE_JSON_BUILD_TESTS}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")