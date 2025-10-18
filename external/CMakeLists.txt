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

add_library(GPUOpen::VulkanMemoryAllocator ALIAS vma)

add_library(argparse INTERFACE)
set(ARGPARSE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/argparse")
target_include_directories(argparse SYSTEM INTERFACE "${ARGPARSE_DIR}/include")

# zstd
set(ZSTD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/KTX-Software/external/basisu/zstd")
add_library(zstd STATIC "${ZSTD_INCLUDE_DIR}/zstd.c")
target_include_directories(zstd SYSTEM INTERFACE ${ZSTD_INCLUDE_DIR})
add_library(ZSTD::ZSTD ALIAS zstd)
set(ZSTD_LIBRARY ZSTD::ZSTD)

# KTX-Software
set(KTX_SOFTWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/KTX-Software")

set(KTX_SOURCES
    "${KTX_SOFTWARE_DIR}/lib/checkheader.c"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/createdfd.c"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/colourspaces.c"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/dfd.h"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/dfd2vk.inl"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/interpretdfd.c"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/printdfd.c"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/queries.c"
    "${KTX_SOFTWARE_DIR}/external/dfdutils/vk2dfd.c"
    "${KTX_SOFTWARE_DIR}/external/etcdec/etcdec.cxx"
    "${KTX_SOFTWARE_DIR}/lib/etcunpack.cxx"
    "${KTX_SOFTWARE_DIR}/lib/filestream.c"
    "${KTX_SOFTWARE_DIR}/lib/filestream.h"
    "${KTX_SOFTWARE_DIR}/lib/formatsize.h"
    "${KTX_SOFTWARE_DIR}/lib/hashlist.c"
    "${KTX_SOFTWARE_DIR}/lib/info.c"
    "${KTX_SOFTWARE_DIR}/lib/ktxint.h"
    "${KTX_SOFTWARE_DIR}/lib/memstream.c"
    "${KTX_SOFTWARE_DIR}/lib/memstream.h"
    "${KTX_SOFTWARE_DIR}/lib/strings.c"
    "${KTX_SOFTWARE_DIR}/lib/swap.c"
    "${KTX_SOFTWARE_DIR}/lib/uthash.h"
    "${KTX_SOFTWARE_DIR}/lib/texture.c"
    "${KTX_SOFTWARE_DIR}/lib/texture.h"
    "${KTX_SOFTWARE_DIR}/utils/unused.h"

     # Basis Universal
    "${KTX_SOFTWARE_DIR}/lib/basis_sgd.h"
    "${KTX_SOFTWARE_DIR}/lib/basis_transcode.cpp"
    "${KTX_SOFTWARE_DIR}/lib/miniz_wrapper.cpp"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu_containers.h"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu_containers_impl.h"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu_file_headers.h"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu_transcoder_internal.h"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu_transcoder_uastc.h"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu_transcoder.cpp"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu_transcoder.h"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder/basisu.h"

    # KT1
    "${KTX_SOFTWARE_DIR}/lib/texture1.c"
    "${KTX_SOFTWARE_DIR}/lib/texture1.h"

    # KTX2
    "${KTX_SOFTWARE_DIR}/lib/texture2.c"
    "${KTX_SOFTWARE_DIR}/lib/texture2.h"

    # Vulkan support
    "${KTX_SOFTWARE_DIR}/lib/vk_format.h"
    "${KTX_SOFTWARE_DIR}/lib/vkformat_check.c"
    "${KTX_SOFTWARE_DIR}/lib/vkformat_check_variant.c"
    "${KTX_SOFTWARE_DIR}/lib/vkformat_enum.h"
    "${KTX_SOFTWARE_DIR}/lib/vkformat_str.c"
    "${KTX_SOFTWARE_DIR}/lib/vkformat_typesize.c"
    "${KTX_SOFTWARE_DIR}/lib/vk_funcs.c"
    "${KTX_SOFTWARE_DIR}/lib/vk_funcs.h"
    "${KTX_SOFTWARE_DIR}/lib/vkloader.c"
)

set(KTX_INCLUDE_DIRS
    "${KTX_SOFTWARE_DIR}/include"
    "${KTX_SOFTWARE_DIR}/lib"
    "${KTX_SOFTWARE_DIR}/utils"
    "${KTX_SOFTWARE_DIR}/external"
    "${KTX_SOFTWARE_DIR}/external/basisu/zstd"
    "${KTX_SOFTWARE_DIR}/external/basisu/transcoder"
    "${KTX_SOFTWARE_DIR}/other_include"
)

add_library(ktx STATIC ${KTX_SOURCES})

target_compile_definitions(ktx PUBLIC LIBKTX)
if (WIN32)
target_compile_definitions(ktx PUBLIC "KTX_API=__declspec(dllexport)")
endif()
target_compile_definitions(ktx PUBLIC KTX_FEATURE_WRITE=0)
target_compile_definitions(ktx PUBLIC BASISD_SUPPORT_KTX2_ZSTD=0)
target_compile_definitions(ktx PUBLIC BASISU_NO_ITERATOR_DEBUG_LEVEL)

target_include_directories(ktx SYSTEM PUBLIC ${KTX_INCLUDE_DIRS})

target_link_libraries(ktx PUBLIC 
    zstd
    vulkan
)

add_library(KTX::ktx ALIAS ktx)

# stb
set(STB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stb")
set(STB_IMAGE_IMPL_DIR "${CMAKE_CURRENT_BINARY_DIR}/stb_impl")

set(STB_IMAGE_IMPL_C "${STB_IMAGE_IMPL_DIR}/stb_image.c")
set(STB_IMAGE_RESIZE2_IMPL_C "${STB_IMAGE_IMPL_DIR}/stb_image_resize2.c")
set(STB_IMAGE_WRITE_IMPL_C "${STB_IMAGE_IMPL_DIR}/stb_image_write.c")

file(WRITE "${STB_IMAGE_IMPL_C}"
"#ifndef STB_IMAGE_IMPLEMENTATION\n"
"#define STB_IMAGE_IMPLEMENTATION\n"
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

# rapidyaml
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/rapidyaml")

# mimalloc
set(MI_OVERRIDE OFF CACHE BOOL "Override the standard malloc interface (i.e. define entry points for 'malloc', 'free', etc)" FORCE)
set(MI_WIN_REDIRECT OFF CACHE BOOL "Use redirection module ('mimalloc-redirect') on Windows if compiling mimalloc as a DLL" FORCE)
set(MI_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "Build test executables" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mimalloc")

add_library(${PROJECT_NAME} INTERFACE)
target_link_libraries(${PROJECT_NAME}
    INTERFACE

    mimalloc-static

    argparse
    spdlog::spdlog
    tiny_imageformat

    KTX::ktx
    stb

    glm::glm

    Vulkan::Headers
    volk::volk
    GPUOpen::VulkanMemoryAllocator

    ryml::ryml
)