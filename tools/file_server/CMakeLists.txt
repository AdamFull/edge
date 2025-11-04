project(file_server)

if(WIN32)
add_executable(${PROJECT_NAME} WIN32 main.cpp)
else()
add_executable(${PROJECT_NAME} main.cpp)
endif()

target_link_libraries(${PROJECT_NAME}
	PRIVATE
    mimalloc-static
	argparse
	spdlog::spdlog
)