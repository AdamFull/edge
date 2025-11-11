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
    mimalloc-static
	argparse
	ryml::ryml
	Vulkan::Headers
	spdlog::spdlog
	slang::slang
	libzstd
)

function(add_shader_compilation TARGET SHADER_SOURCE OUTPUT_FILE)
    set(DEPFILE "${OUTPUT_FILE}.d")
    
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND $<TARGET_FILE:shader_compiler>
                -i ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}
                -o ${OUTPUT_FILE}
                --depfile ${DEPFILE}
                ${ARGN}
        DEPENDS ${SHADER_SOURCE} shader_compiler
        DEPFILE ${DEPFILE}
        COMMENT "Compiling shader: ${SHADER_SOURCE}"
        VERBATIM
    )
    
    target_sources(${TARGET} PRIVATE ${OUTPUT_FILE})
endfunction()