cmake_minimum_required(VERSION 3.12)

project(libedgehttp
    VERSION 1.0.0
    DESCRIPTION "Edge foundation http wrapper under libcurl"
    LANGUAGES C
)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Fetch libcurl
include(FetchContent)

FetchContent_Declare(
    curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_17_0
    GIT_SHALLOW TRUE
)

set(BUILD_CURL_EXE OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(CURL_STATICLIB ON CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_EXAMPLES OFF CACHE INTERNAL "")

set(CURL_DISABLE_LDAP ON CACHE INTERNAL "")
set(CURL_DISABLE_LDAPS ON CACHE INTERNAL "")
set(CURL_USE_LIBPSL OFF CACHE INTERNAL "")
set(CURL_USE_LIBSSH2 OFF CACHE INTERNAL "")
set(CURL_ENABLE_SSL ON CACHE INTERNAL "")

set(HTTP_ONLY ON CACHE INTERNAL "")
set(CURL_DISABLE_COOKIES OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(curl)

# Build options
option(EDGE_HTTP_BUILD_SHARED "Build shared library" OFF)
option(EDGE_HTTP_BUILD_STATIC "Build static library" ON)
option(EDGE_HTTP_BUILD_EXAMPLES "Build examples" OFF)

# Library sources
set(EDGE_HTTP_SOURCES
    "src/edge_http.c"
    "src/edge_http_websocket.c"
)

set(EDGE_HTTP_HEADERS
    "include/edge_http.h"
    "include/edge_http_websocket.h"
)

# Shared library
if(EDGE_HTTP_BUILD_SHARED)
    add_library(edge_http SHARED ${EDGE_HTTP_SOURCES})
    target_include_directories(edge_http
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(edge_http
        PUBLIC 
        CURL::libcurl 
        edge_base
    )
    
    set_target_properties(edge_http PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${EDGE_HTTP_HEADERS}"
    )
endif()

# Static library
if(EDGE_HTTP_BUILD_STATIC)
    add_library(edge_http_static STATIC ${EDGE_HTTP_SOURCES})
    target_include_directories(edge_http_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(edge_http_static
        PUBLIC
        CURL::libcurl
        edge_base
    )
    
    set_target_properties(edge_http_static PROPERTIES
        OUTPUT_NAME edge_http
        PUBLIC_HEADER "${EDGE_HTTP_HEADERS}"
    )
endif()

# Examples
if(EDGE_HTTP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

if(EDGE_HTTP_BUILD_SHARED)
    install(TARGETS edge_http
        EXPORT edgeHttpTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

if(EDGE_HTTP_BUILD_STATIC)
    install(TARGETS edge_http_static
        EXPORT edgeHttpTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

# Export targets
install(EXPORT edgeHttpTargets
    FILE httpcTargets.cmake
    NAMESPACE edge::http::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_http
)

# Generate config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edgeHttpConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edgeHttpConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_http
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edgeHttpConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edgeHttpConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edgeHttpConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edge_http
)

# Pkg-config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/edge_http.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/edge_http.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edge_http.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "libedge_http Configuration Summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build shared:     ${EDGE_HTTP_BUILD_SHARED}")
message(STATUS "  Build static:     ${EDGE_HTTP_BUILD_STATIC}")
message(STATUS "  Build examples:   ${EDGE_HTTP_BUILD_EXAMPLES}")
message(STATUS "  CURL found:       ${CURL_FOUND}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")