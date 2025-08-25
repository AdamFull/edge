add_subdirectory("external")

set(ENABLE_VULKAN_VALIDATION_LAYERS ON CACHE BOOL "Enable validation layers for every application.")
set(ENABLE_VULKAN_VALIDATION_LAYERS_GPU_ASSISTED OFF CACHE BOOL "Enable GPU assisted validation layers for every application.")
set(ENABLE_VULKAN_VALIDATION_LAYERS_BEST_PRACTICES OFF CACHE BOOL "Enable best practices validation layers for every application.")
set(ENABLE_VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION ON CACHE BOOL "Enable synchronization validation layers for every application.")
set(ENABLE_VULKAN_DEBUG ON CACHE BOOL "Enable VK_EXT_debug_utils or VK_EXT_debug_marker if supported.")

if(${ENABLE_VULKAN_DEBUG})
    if(${ENABLE_VULKAN_VALIDATION_LAYERS})
        target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_VALIDATION_LAYERS)

           # GPU assisted validation layers are not available on macOS.
        if(${ENABLE_VULKAN_VALIDATION_LAYERS_GPU_ASSISTED})
            if (APPLE)
                message(WARNING "GPU assisted validation layers are not currently available on macOS.")
            else()
                target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_VALIDATION_LAYERS_GPU_ASSISTED)
            endif()
        endif()
        
        if(${ENABLE_VULKAN_VALIDATION_LAYERS_BEST_PRACTICES})
            target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_VALIDATION_LAYERS_BEST_PRACTICES)
        endif()
        
        if(${ENABLE_VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION} AND NOT ANDROID)
            target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION)
        endif()
    endif()

    target_compile_definitions(${PROJECT_NAME} PUBLIC VULKAN_DEBUG)
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

if(USE_NSIGHT_AFTERMATH)
    target_link_libraries(${PROJECT_NAME} PRIVATE nsight_aftermath_interface)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    vulkan_externals
)

target_sources(${PROJECT_NAME} PRIVATE
	"vk_context.cpp"
	"vk_primitives.cpp"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    VK_NO_PROTOTYPES
)