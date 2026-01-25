project(edge_externals LANGUAGES C CXX)

include(FetchContent)

find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
    # Fetch zlib
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
    )
    FetchContent_MakeAvailable(zlib)
endif()

find_package(zstd QUIET)

if(NOT zstd_FOUND)
    FetchContent_Declare(
        zstd
        GIT_REPOSITORY https://github.com/facebook/zstd.git
        GIT_TAG        v1.5.6
        SOURCE_SUBDIR  build/cmake
    )
    
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(zstd)
    
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(libzstd_static PRIVATE -mbmi2 -mlzcnt)
    endif()
endif()

FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG v4.1.4
)

set(FLECS_STATIC ON CACHE BOOL "" FORCE)
set(FLECS_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(flecs)

# vulkan
add_library(vulkan INTERFACE)
set(VULKAN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/include)
target_sources(vulkan INTERFACE 
    ${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h
    ${VULKAN_INCLUDE_DIR}/vulkan/vulkan.hpp
)
target_include_directories(vulkan SYSTEM INTERFACE ${VULKAN_INCLUDE_DIR})

target_compile_definitions(vulkan INTERFACE VK_NO_PROTOTYPES)

if(ANDROID)
    target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_ANDROID_KHR)
elseif(WIN32)
    target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
	target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_METAL_EXT)
elseif(UNIX)
    set(EDGE_VK_WSI_SELECTION XLIB)
    # Choose WSI based on EDGE_VK_WSI_SELECTION
    if (VKB_WSI_SELECTION STREQUAL XCB OR EDGE_VK_WSI_SELECTION STREQUAL XLIB OR EDGE_VK_WSI_SELECTION STREQUAL WAYLAND)
        find_package(PkgConfig REQUIRED)
    endif()
    if (EDGE_VK_WSI_SELECTION STREQUAL XCB)
        pkg_check_modules(XCB xcb REQUIRED)
        if (XCB_FOUND)
            target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XCB_KHR)
        endif()
    elseif (EDGE_VK_WSI_SELECTION STREQUAL XLIB)
        pkg_check_modules(X11 x11 REQUIRED)
        if (X11_FOUND)
            target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_XLIB_KHR)
        endif()
    elseif (EDGE_VK_WSI_SELECTION STREQUAL WAYLAND)
        pkg_check_modules(WAYLAND wayland-client REQUIRED)
        if (WAYLAND_FOUND)
            target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_WAYLAND_KHR)
        endif()
    elseif (EDGE_VK_WSI_SELECTION STREQUAL D2D)
        set(DIRECT_TO_DISPLAY TRUE)
        set(DIRECT_TO_DISPLAY TRUE PARENT_SCOPE)
        target_compile_definitions(vulkan INTERFACE VK_USE_PLATFORM_DISPLAY_KHR)
    else()
        message(FATAL_ERROR "Unknown WSI")
    endif()
endif()

add_library(Vulkan::Headers ALIAS vulkan)

# volk
set(VOLK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/volk")
set(VOLK_FILES
    "${VOLK_DIR}/volk.c"
    "${VOLK_DIR}/volk.h")

add_library(volk STATIC ${VOLK_FILES})
set_target_properties(volk PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(volk PUBLIC vulkan)

target_include_directories(volk SYSTEM PUBLIC ${VOLK_DIR})
if (EDGE_VK_WSI_SELECTION STREQUAL XCB)
    target_include_directories(volk SYSTEM PUBLIC ${XCB_INCLUDE_DIRS})
elseif (EDGE_VK_WSI_SELECTION STREQUAL XLIB)
    target_include_directories(volk SYSTEM PUBLIC ${X11_INCLUDE_DIRS})
elseif (EDGE_VK_WSI_SELECTION STREQUAL WAYLAND)
    target_include_directories(volk SYSTEM PUBLIC ${WAYLAND_INCLUDE_DIRS})
endif()

add_library(volk::volk ALIAS volk)

# vma
set(VMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vma/include")
set(VMA_IMPL_DIR "${CMAKE_CURRENT_BINARY_DIR}/vma_impl")
set(VMA_IMPL_C "${VMA_IMPL_DIR}/vk_mem_alloc_impl.cpp")

file(WRITE "${VMA_IMPL_C}"
"#ifndef VMA_IMPLEMENTATION\n"
"#define VMA_IMPLEMENTATION\n"
"#include \"vk_mem_alloc.h\"\n"
"#endif\n"
)

add_library(vma STATIC ${VMA_IMPL_C})
target_include_directories(vma PUBLIC ${VMA_DIR})
target_link_libraries(vma PUBLIC vulkan)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(vma PRIVATE -Wno-nullability-completeness)
endif()

add_library(GPUOpen::VulkanMemoryAllocator ALIAS vma)

# mimalloc
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mimalloc")

# stb
set(STB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stb")
set(STB_IMAGE_IMPL_DIR "${CMAKE_CURRENT_BINARY_DIR}/stb_impl")

set(STB_IMAGE_IMPL_C "${STB_IMAGE_IMPL_DIR}/stb_image.c")
set(STB_IMAGE_RESIZE2_IMPL_C "${STB_IMAGE_IMPL_DIR}/stb_image_resize2.c")
set(STB_IMAGE_WRITE_IMPL_C "${STB_IMAGE_IMPL_DIR}/stb_image_write.c")

file(WRITE "${STB_IMAGE_IMPL_C}"
"#ifndef STB_IMAGE_IMPLEMENTATION\n"
"#define STB_IMAGE_IMPLEMENTATION\n"
"#define USE_MIMALLOC"
"#ifdef USE_MIMALLOC"
"#include <mimalloc.h>"
"#define STBI_MALLOC(sz) mi_malloc(sz)"
"#define STBI_REALLOC(p,newsz) mi_realloc(p,newsz)"
"#define STBI_FREE(p) mi_free(p)"
"#endif"
"#include \"stb_image.h\"\n"
"#endif\n"
)

file(WRITE "${STB_IMAGE_RESIZE2_IMPL_C}"
"#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION\n"
"#define STB_IMAGE_RESIZE_IMPLEMENTATION\n"
"#include \"stb_image_resize2.h\"\n"
"#endif\n"
)

file(WRITE "${STB_IMAGE_WRITE_IMPL_C}"
"#ifndef STB_IMAGE_WRITE_IMPLEMENTATION\n"
"#define STB_IMAGE_WRITE_IMPLEMENTATION\n"
"#include \"stb_image_write.h\"\n"
"#endif\n"
)

add_library(stb STATIC
    ${STB_IMAGE_IMPL_C}
    ${STB_IMAGE_RESIZE2_IMPL_C}
    ${STB_IMAGE_WRITE_IMPL_C}
)

target_link_libraries(stb PRIVATE mimalloc-static)
target_include_directories(stb PUBLIC ${STB_DIR})

# glm
add_library(glm INTERFACE)
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glm")
target_sources(glm INTERFACE "${GLM_DIR}/glm/glm.hpp")
target_include_directories(glm SYSTEM INTERFACE ${GLM_DIR})

target_compile_definitions(glm INTERFACE
    GLM_FORCE_SWIZZLE
    GLM_FORCE_RADIANS
    GLM_FORCE_CTOR_INIT
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_definitions(glm INTERFACE GLM_FORCE_CXX20)
endif()

add_library(glm::glm ALIAS glm)

## freetype
#add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/freetype")

# imgui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(IMGUI_FILES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp"
    #"${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp"
)

add_library(imgui STATIC ${IMGUI_FILES})
target_compile_definitions(imgui PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_USE_WCHAR32
    #IMGUI_ENABLE_FREETYPE
)
target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR})
#target_link_libraries(imgui PRIVATE freetype)

add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME}
    INTERFACE

    mimalloc-static

    libzstd_static

    stb

    glm::glm

    Vulkan::Headers
    volk::volk
    GPUOpen::VulkanMemoryAllocator

    imgui

    flecs::flecs_static
)