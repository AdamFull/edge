include(FetchContent)

target_sources(${PROJECT_NAME}
    PRIVATE
    "input_events.c"
)

# Platform detection and definitions
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        EDGE_PLATFORM_WINDOWS=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        GLFW_EXPOSE_NATIVE_WIN32
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_WIN64=1)
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_WIN32=1)
    endif()
elseif(ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_ANDROID=1)
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        EDGE_PLATFORM_LINUX=1
        GLFW_EXPOSE_NATIVE_X11
    )
endif()

# Platform categories
if(WIN32 OR UNIX AND NOT ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_DESKTOP=1)
elseif(ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_MOBILE=1)
endif()

# Desktop platforms (Windows, Linux, macOS)
if(WIN32 OR (UNIX AND NOT ANDROID AND NOT IOS))
    # GLFW setup for desktop platforms    
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    FetchContent_MakeAvailable(glfw)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)
endif()

if(WIN32)
    # Windows-specific sources
    target_sources(${PROJECT_NAME} PRIVATE 
        "windows_platform.c"
    )
elseif(ANDROID)
    find_package(games-controller REQUIRED CONFIG)
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            games-controller::paddleboat_static
    )

    # Android-specific sources
    target_sources(${PROJECT_NAME} PRIVATE
        "android_platform.cpp"
    )
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    # Linux-specific sources
    target_sources(${PROJECT_NAME} PRIVATE 
        #"linux_context.cpp"
        #"unix_filesystem.cpp"
    )
else()
endif()
