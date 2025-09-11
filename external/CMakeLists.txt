project(edge_externals LANGUAGES C CXX)

include(FetchContent)

# Fetch spdlog
if(NOT WIN32)
    set(SPDLOG_BUILD_SHARED ON CACHE BOOL "Build shared library")
endif()

set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "Use std::format instead of fmt")

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.3
)
FetchContent_MakeAvailable(spdlog)

# tiny_imageformat
set(TINY_IMAGEFORMAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tiny_imageformat)

add_library(tiny_imageformat INTERFACE)
target_sources(tiny_imageformat INTERFACE
    "${TINY_IMAGEFORMAT_DIR}/include/tiny_imageformat/tinyimageformat_base.h"
	"${TINY_IMAGEFORMAT_DIR}/include/tiny_imageformat/tinyimageformat_apis.h"
	"${TINY_IMAGEFORMAT_DIR}/include/tiny_imageformat/tinyimageformat_bits.h"
	"${TINY_IMAGEFORMAT_DIR}/include/tiny_imageformat/tinyimageformat_decode.h"
	"${TINY_IMAGEFORMAT_DIR}/include/tiny_imageformat/tinyimageformat_encode.h"
	"${TINY_IMAGEFORMAT_DIR}/include/tiny_imageformat/tinyimageformat_query.h"
)
target_include_directories(tiny_imageformat SYSTEM INTERFACE ${TINY_IMAGEFORMAT_DIR}/include)

add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME}
    INTERFACE
    spdlog::spdlog
    tiny_imageformat
)