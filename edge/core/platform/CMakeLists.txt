include(FetchContent)

target_sources(${PROJECT_NAME} PRIVATE "platform.cpp")

if(WIN32 OR LINUX)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.9 # Or a specific commit hash
)
FetchContent_MakeAvailable(glfw)

target_link_libraries(${PROJECT_NAME} glfw)
endif()