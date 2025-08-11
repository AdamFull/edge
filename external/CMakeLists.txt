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

add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME}
    INTERFACE
    spdlog::spdlog
)