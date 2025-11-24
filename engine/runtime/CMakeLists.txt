include(FetchContent)

target_sources(${PROJECT_NAME}
    PRIVATE
    "input_events.c"
)

# Platform categories
if(WIN32 OR UNIX AND NOT ANDROID)
elseif(ANDROID)
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
