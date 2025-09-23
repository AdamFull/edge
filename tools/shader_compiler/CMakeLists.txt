project(shader_compiler)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/slang.2025.17")
find_package(slang REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	Vulkan::Headers
	spdlog::spdlog
	slang::slang
)