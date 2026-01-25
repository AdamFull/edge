cmake_minimum_required(VERSION 3.10)
project(libedgeimage VERSION 0.0.1 LANGUAGES C CXX)

find_package(lz4 QUIET)

if(NOT lz4_FOUND)
  FetchContent_Declare(
      lz4
      GIT_REPOSITORY https://github.com/lz4/lz4.git
      GIT_TAG        v1.10.0
      SOURCE_SUBDIR  build/cmake
  )

  FetchContent_MakeAvailable(lz4)
endif()

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(EDGE_IMAGE_BUILD_EXAMPLES "Build examples" ON)

# Library source files
set(EDGE_IMAGE_SOURCES
    "src/image.cpp"
)

set(EDGE_IMAGE_HEADERS
    "include/image.hpp"
)

add_library(edge_image STATIC ${EDGE_IMAGE_SOURCES})
target_include_directories(edge_image
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(edge_image PRIVATE ${ARCH_DEFINE})

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(edge_image PRIVATE -m64)
endif()

target_link_libraries(edge_image 
    PRIVATE 
    edge_base
    lz4_static
    Vulkan::Headers
)

set_target_properties(edge_image PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${EDGE_IMAGE_HEADERS}"
)

# Examples
if(EDGE_IMAGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "libedgeimage Configuration Summary:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build examples:   ${EDGE_IMAGE_BUILD_EXAMPLES}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")