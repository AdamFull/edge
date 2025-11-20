project(edge_engine LANGUAGES C)

set(EDGE_ENGINE_SOURCES
    "main.c"
)

if(ANDROID)
    find_package(game-activity REQUIRED CONFIG)

    add_library(${PROJECT_NAME} SHARED ${EDGE_ENGINE_SOURCES})

    target_link_libraries(${PROJECT_NAME} 
        PUBLIC
        log android 
        game-activity::game-activity_static
    )
elseif(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${EDGE_ENGINE_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${EDGE_ENGINE_SOURCES})
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
target_compile_options(${PROJECT_NAME} PRIVATE -mavx2)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
    $<$<CONFIG:Debug>:EDGE_DEBUG>
    $<$<CONFIG:Release>:EDGE_RELEASE>
)

add_subdirectory(runtime)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    edge_externals
    edge_foundation
)