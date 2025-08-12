
option(USE_NSIGHT_AFTERMATH OFF)

if (USE_NSIGHT_AFTERMATH)
find_package(NsightAftermath REQUIRED)
add_library(nsight_aftermath_interface INTERFACE)
target_compile_definitions(nsight_aftermath_interface INTERFACE USE_NSIGHT_AFTERMATH = 1)
target_link_libraries(nsight_aftermath_interface INTERFACE NsightAftermath::NsightAftermath)

target_link_libraries(${PROJECT_NAME} PUBLIC nsight_aftermath_interface)
endif()

target_sources(${PROJECT_NAME} PRIVATE
	"context.cpp"
)

if(WIN32 OR ANDROID)
	target_compile_definitions(${PROJECT_NAME} PRIVATE EDGE_GFX_BACKEND_VULKAN=1)
	add_subdirectory("vulkan")
endif()