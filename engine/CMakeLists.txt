project(edge LANGUAGES C CXX)

set(ENABLE_VULKAN_VALIDATION_LAYERS ON CACHE BOOL "Enable validation layers for every application.")
set(ENABLE_VULKAN_VALIDATION_LAYERS_GPU_ASSISTED OFF CACHE BOOL "Enable GPU assisted validation layers for every application.")
set(ENABLE_VULKAN_VALIDATION_LAYERS_BEST_PRACTICES OFF CACHE BOOL "Enable best practices validation layers for every application.")
set(ENABLE_VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION ON CACHE BOOL "Enable synchronization validation layers for every application.")
set(ENABLE_VULKAN_DEBUG ON CACHE BOOL "Enable VK_EXT_debug_utils or VK_EXT_debug_marker if supported.")

set(EDGE_ENGINE_SOURCES
    "event_dispatcher.c"
    "gfx_context.c"
    "gfx_renderer.c"
    "main.c"
)

if(ANDROID)
    find_package(game-activity REQUIRED CONFIG)

    add_library(${PROJECT_NAME} SHARED ${EDGE_ENGINE_SOURCES})

    target_link_libraries(${PROJECT_NAME} 
        PUBLIC
        log android 
        game-activity::game-activity_static
    )
elseif(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${EDGE_ENGINE_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${EDGE_ENGINE_SOURCES})
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
target_compile_options(${PROJECT_NAME} PRIVATE -mavx2)
endif()

if (USE_NSIGHT_AFTERMATH)
find_package(NsightAftermath REQUIRED)
add_library(nsight_aftermath_interface INTERFACE)
target_compile_definitions(nsight_aftermath_interface INTERFACE USE_NSIGHT_AFTERMATH = 1)
target_link_libraries(nsight_aftermath_interface INTERFACE NsightAftermath::NsightAftermath)

target_link_libraries(${PROJECT_NAME} PUBLIC nsight_aftermath_interface)
endif()

if(${ENABLE_VULKAN_DEBUG})
    if(${ENABLE_VULKAN_VALIDATION_LAYERS})
        target_compile_definitions(${PROJECT_NAME} PUBLIC EDGE_VK_USE_VALIDATION_LAYERS)

           # GPU assisted validation layers are not available on macOS.
        if(${ENABLE_VULKAN_VALIDATION_LAYERS_GPU_ASSISTED})
            if (APPLE)
                message(WARNING "GPU assisted validation layers are not currently available on macOS.")
            else()
                target_compile_definitions(${PROJECT_NAME} PUBLIC EDGE_VK_USE_GPU_ASSISTED_VALIDATION)
            endif()
        endif()
        
        if(${ENABLE_VULKAN_VALIDATION_LAYERS_BEST_PRACTICES})
            target_compile_definitions(${PROJECT_NAME} PUBLIC EDGE_VK_USE_BEST_PRACTICES_VALIDATION)
        endif()
        
        if(${ENABLE_VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION} AND NOT ANDROID)
            target_compile_definitions(${PROJECT_NAME} PUBLIC EDGE_VK_USE_SYNCHRONIZATION_VALIDATION)
        endif()
    endif()
endif()

if(${ENABLE_VULKAN_PORTABILITY})
    message(STATUS "Vulkan Portability Enumeration and Portability Subset extensions are enabled")
    target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_ENABLE_PORTABILITY)
endif()

target_compile_definitions(${PROJECT_NAME} 
    PRIVATE
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    VMA_STATS_STRING_ENABLED=0 
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    VK_NO_PROTOTYPES
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    VMA_STATS_STRING_ENABLED=0 
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_DISABLE_ENHANCED_MODE=1
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    $<$<CONFIG:Debug>:EDGE_DEBUG>
    $<$<CONFIG:Release>:EDGE_RELEASE>
)

add_subdirectory(runtime)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    edge_externals
    edge_foundation
)