include(FetchContent)

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
elseif(EMSCRIPTEN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_EMSCRIPTEN=1)
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_LINUX=1)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_APPLE=1)
    if(IOS)
        target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_IOS=1)
    else()
        target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_OSX=1)
    endif()
endif()

# Platform categories
if(WIN32 OR UNIX AND NOT ANDROID AND NOT IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_DESKTOP=1)
elseif(ANDROID OR IOS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_PLATFORM_MOBILE=1)
endif()

target_sources(${PROJECT_NAME} PRIVATE 
        "platform.cpp"
    )

# Desktop platforms (Windows, Linux, macOS)
if(WIN32 OR (UNIX AND NOT ANDROID AND NOT IOS))
    # GLFW setup for desktop platforms
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.3.9
    )
    FetchContent_MakeAvailable(glfw)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)
    
    # Desktop window implementation
    target_sources(${PROJECT_NAME} PRIVATE 
        "desktop_input.cpp"
        "desktop_window.cpp"
    )
endif()

if(WIN32)
    # Windows-specific sources
    target_sources(${PROJECT_NAME} PRIVATE 
        "windows_context.cpp"
        "windows_frame_handler.cpp"
        "windows_filesystem.cpp"
        "zip_filesystem.cpp"
    )
elseif(ANDROID)
    find_package(games-controller REQUIRED CONFIG)
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            games-controller::paddleboat_static
    )

    # Android-specific sources
    target_sources(${PROJECT_NAME} PRIVATE
        #"${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c"
        "android_frame_handler.cpp"
        "android_jni_helper.cpp"
        "android_context.cpp"
        "android_input.cpp"
        "android_window.cpp"
        "unix_filesystem.cpp"
        "zip_filesystem.cpp"
    )
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    # Linux-specific sources
    target_sources(${PROJECT_NAME} PRIVATE 
        "unix_filesystem.cpp"
    )
else()
    target_sources(${PROJECT_NAME} PRIVATE 
        "generic_frame_handler.cpp"
    )
endif()
