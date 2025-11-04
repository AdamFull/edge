cmake_minimum_required(VERSION 3.22.1)
project(edge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(APP_SOURCES
    "edge/engine.cpp"
    "edge/imgui_layer.cpp"
    "main.cpp"
)

if(ANDROID)
    find_package(game-activity REQUIRED CONFIG)

    add_library(${PROJECT_NAME} SHARED ${APP_SOURCES})

    target_link_libraries(${PROJECT_NAME} 
        PUBLIC
        log android 
        game-activity::game-activity_static
    )
elseif(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${APP_SOURCES})
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
    $<$<CONFIG:Debug>:EDGE_DEBUG>
    $<$<CONFIG:Release>:EDGE_RELEASE>
)

add_subdirectory("external")
add_subdirectory("edge/core")
#add_subdirectory("edge/graphics")

target_link_libraries(${PROJECT_NAME} 
        PUBLIC
        edge_externals
    )

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# tools
if(WIN32)
add_subdirectory("tools/file_server")

add_subdirectory("tools/shader_compiler")
add_dependencies(edge shader_compiler)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/assets/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

add_shader_compilation(edge "assets/shaders/imgui.technique.yaml" "${SHADER_OUTPUT_DIR}/imgui.shfx")
add_shader_compilation(edge "assets/shaders/fullscreen.technique.yaml" "${SHADER_OUTPUT_DIR}/fullscreen.shfx")
endif(WIN32)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts DESTINATION ${CMAKE_BINARY_DIR}/assets)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets/images DESTINATION ${CMAKE_BINARY_DIR}/assets)

