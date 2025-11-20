cmake_minimum_required(VERSION 3.10)
project(libedgejobcoro VERSION 0.0.1 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library source files
set(EDGE_LOGGER_SOURCES
    "src/edge_logger.c"
    "src/edge_logger_file_output.c"
    "src/edge_logger_stdout_output.c"
)

set(EDGE_LOGGER_HEADERS
    "include/edge_logger.h"
)

if(WIN32)
    set(EDGE_LOGGER_SOURCES ${EDGE_LOGGER_SOURCES} "src/edge_logger_debug_output.c")
elseif(ANDROID)
    set(EDGE_LOGGER_SOURCES ${EDGE_LOGGER_SOURCES} "src/edge_logger_logcat_output.c")
endif()

add_library(edge_logger STATIC ${EDGE_LOGGER_SOURCES})
target_include_directories(edge_logger
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(edge_base PRIVATE _CRT_SECURE_NO_WARNINGS)

if(ANDROID)
target_link_libraries(edge_logger
        PUBLIC
        log android 
    )
endif()

target_link_libraries(edge_logger 
    PRIVATE 
    edge_base
)

set_target_properties(edge_logger PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${EDGE_LOGGER_HEADERS}"
)

# Installation
include(GNUInstallDirs)

install(TARGETS edge_logger
    EXPORT edgeLoggerTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT edgeLoggerTargets
    FILE edgeLoggerTargets.cmake
    NAMESPACE edge::coro::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_logger
)

# Generate config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edgeLoggerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edgeLoggerConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_logger
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edgeLoggerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edgeLoggerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edgeLoggerConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_logger
)

# Pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edge_logger.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/edge_logger.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edge_logger.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "libedgelogger Configuration Summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")