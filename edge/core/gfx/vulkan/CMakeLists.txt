include(FetchContent)

# Fetch Vulkan-Headers v1.4.309
FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.4.309
)

# Fetch VulkanMemoryAllocator v3.3.0
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
)

# Fetch Volk vulkan-sdk-1.4.309.0
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG vulkan-sdk-1.4.309.0
)

# Configure VulkanMemoryAllocator - disable samples and examples
set(VMA_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(VMA_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)

# Configure volk - disable examples
set(VOLK_INSTALL OFF CACHE BOOL "" FORCE)

# Make the content available
FetchContent_MakeAvailable(VulkanHeaders VulkanMemoryAllocator volk)

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
    Vulkan::Headers
    GPUOpen::VulkanMemoryAllocator
    volk
)

target_sources(${PROJECT_NAME} PRIVATE
	"vk_context.cpp"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    VK_NO_PROTOTYPES
)

if(ANDROID)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
elseif(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_METAL_EXT)
elseif(UNIX)
    #TODO
endif()