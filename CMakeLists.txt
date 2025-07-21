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

set(APP_SOURCES main.cpp)

if(ANDROID)
    add_library(${PROJECT_NAME} SHARED ${APP_SOURCES})
elseif(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${APP_SOURCES})
endif()

add_subdirectory("edge/platform")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)