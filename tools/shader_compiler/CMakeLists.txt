project(shader_compiler)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/slang.2025.17")
find_package(slang REQUIRED)

if(WIN32)
add_executable(${PROJECT_NAME} WIN32 main.cpp)
else()
add_executable(${PROJECT_NAME} main.cpp)
endif()

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	argparse
	Vulkan::Headers
	spdlog::spdlog
	slang::slang
)