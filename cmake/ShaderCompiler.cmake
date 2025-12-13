# Find slangc compiler
find_program(SLANGC_EXECUTABLE slangc
    HINTS
        "$ENV{SLANG_DIR}/bin"
        "$ENV{SLANG_HOME}/bin"
        "$ENV{VULKAN_SDK}/bin"
        "/usr/local/bin"
        "/usr/bin"
    DOC "Slang shader compiler (slangc)"
)

if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc compiler not found. Please install Slang or set SLANG_DIR environment variable.")
endif()

message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")

set(SLANG_DEFAULT_FLAGS
    -target spirv
    -fvk-use-dx-layout
    -fvk-use-scalar-layout
)

function(add_shader_compile SHADER_SOURCE)
    set(ARGS_OPTIONS "")
    set(ARGS_ONE EMBED_TARGET ENTRY_POINT STAGE OUTPUT_FILE)
    set(ARGS_MULTI SLANGC_FLAGS INCLUDE_DIRECTORIES DEFINES)

    cmake_parse_arguments(PARSE_ARGV 1 OPTION "${ARGS_OPTIONS}" "${ARGS_ONE}" "${ARGS_MULTI}")
    
    if(NOT OPTION_EMBED_TARGET)
        message(FATAL_ERROR "add_shader_compile: EMBED_TARGET is required")
    endif()

    cmake_path(GET SHADER_SOURCE PARENT_PATH SHADER_REL_DIR)
    cmake_path(RELATIVE_PATH SHADER_REL_DIR)
    cmake_path(GET SHADER_SOURCE FILENAME SHADER_FILENAME)
    cmake_path(SET SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_REL_DIR})
    cmake_path(SET SHADER_SOURCE_FILE ${SHADER_SOURCE_DIR}/${SHADER_FILENAME})

    if(NOT EXISTS ${SHADER_SOURCE_FILE})
        message(FATAL_ERROR "Could not find shader source file: ${SHADER_SOURCE_FILE}")
    endif()

    set(SLANGC_INCLUDE_FLAGS "")
    foreach(inc_dir ${OPTION_INCLUDE_DIRECTORIES})
        cmake_path(SET abs_inc_dir NORMALIZE ${CMAKE_CURRENT_SOURCE_DIR}/${inc_dir})
        list(APPEND SLANGC_INCLUDE_FLAGS -I${abs_inc_dir})
    endforeach()

    set(SLANGC_DEFINE_FLAGS "")
    foreach(def ${OPTION_DEFINES})
        list(APPEND SLANGC_DEFINE_FLAGS -D${def})
    endforeach()

    set(SLANGC_ENTRY_FLAGS "")
    if(OPTION_ENTRY_POINT)
        list(APPEND SLANGC_ENTRY_FLAGS -entry ${OPTION_ENTRY_POINT})
    endif()

    set(SLANGC_STAGE_FLAGS "")
    if(OPTION_STAGE)
        list(APPEND SLANGC_STAGE_FLAGS -stage ${OPTION_STAGE})
    endif()

    set(SLANGC_FLAGS
        ${SLANG_DEFAULT_FLAGS}
        ${OPTION_SLANGC_FLAGS}
        ${SLANGC_INCLUDE_FLAGS}
        ${SLANGC_DEFINE_FLAGS}
        ${SLANGC_ENTRY_FLAGS}
        ${SLANGC_STAGE_FLAGS}
    )

    if(OPTION_OUTPUT_FILE)
        set(SHADER_SPV ${OPTION_OUTPUT_FILE})
    else()
        set(SHADER_SPV ${OPTION_EMBED_TARGET}.spv)
    endif()
    
    set(SHADER_INC ${OPTION_EMBED_TARGET}.inc)
    set(SHADER_H ${OPTION_EMBED_TARGET}.h)
    set(SHADER_C ${OPTION_EMBED_TARGET}.c)
    
    cmake_path(SET SHADER_SPV_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SPV})
    cmake_path(SET SHADER_INC_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_INC})
    cmake_path(SET SHADER_H_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_H})
    cmake_path(SET SHADER_C_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_C})

    set(h_file_src "
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern \"C\" {
#endif

extern const uint32_t ${OPTION_EMBED_TARGET}[];
extern const size_t ${OPTION_EMBED_TARGET}_size;

#ifdef __cplusplus
}
#endif
")

    set(h_file "")
    if(EXISTS ${SHADER_H_FILE})
        file(READ ${SHADER_H_FILE} h_file)
    endif()

    if(NOT h_file STREQUAL h_file_src)
        file(WRITE ${SHADER_H_FILE} "${h_file_src}")
    endif()

    set(c_file_src "
#include \"${SHADER_H}\"

const uint32_t ${OPTION_EMBED_TARGET}[] = {
#include \"${SHADER_INC}\"
};
const size_t ${OPTION_EMBED_TARGET}_size = sizeof(${OPTION_EMBED_TARGET});
")

    set(c_file "")
    if(EXISTS ${SHADER_C_FILE})
        file(READ ${SHADER_C_FILE} c_file)
    endif()

    if(NOT c_file STREQUAL c_file_src)
        file(WRITE ${SHADER_C_FILE} "${c_file_src}")
    endif()

    add_custom_command(
        OUTPUT ${SHADER_SPV_FILE}
        DEPENDS ${SHADER_SOURCE_FILE}
        COMMAND ${SLANGC_EXECUTABLE}
            ${SLANGC_FLAGS}
            ${SHADER_SOURCE_FILE}
            -o ${SHADER_SPV_FILE}
        COMMENT "Compiling Slang shader: ${SHADER_FILENAME}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${SHADER_INC_FILE}
        DEPENDS ${SHADER_SPV_FILE}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${SHADER_SPV_FILE}
            -DOUTPUT_FILE=${SHADER_INC_FILE}
            -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BinaryToArray.cmake
        COMMENT "Generating include file: ${SHADER_INC}"
        VERBATIM
    )

    set(SHADER_INC_TARGET ${OPTION_EMBED_TARGET}-Inc)
    add_custom_target(${SHADER_INC_TARGET}
        DEPENDS ${SHADER_INC_FILE}
    )

    add_library(${OPTION_EMBED_TARGET} STATIC ${SHADER_H_FILE} ${SHADER_C_FILE})
    target_include_directories(${OPTION_EMBED_TARGET}
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    )
    add_dependencies(${OPTION_EMBED_TARGET} ${SHADER_INC_TARGET})

endfunction()

function(add_shader_compile_multi SHADER_SOURCE)
    set(ARGS_OPTIONS "")
    set(ARGS_ONE 
        VERTEX_TARGET VERTEX_ENTRY
        FRAGMENT_TARGET FRAGMENT_ENTRY
        COMPUTE_TARGET COMPUTE_ENTRY
    )
    set(ARGS_MULTI SLANGC_FLAGS INCLUDE_DIRECTORIES DEFINES)

    cmake_parse_arguments(PARSE_ARGV 1 OPTION "${ARGS_OPTIONS}" "${ARGS_ONE}" "${ARGS_MULTI}")

    if(OPTION_VERTEX_TARGET AND OPTION_VERTEX_ENTRY)
        add_shader_compile(${SHADER_SOURCE}
            EMBED_TARGET ${OPTION_VERTEX_TARGET}
            ENTRY_POINT ${OPTION_VERTEX_ENTRY}
            STAGE vertex
            SLANGC_FLAGS ${OPTION_SLANGC_FLAGS}
            INCLUDE_DIRECTORIES ${OPTION_INCLUDE_DIRECTORIES}
            DEFINES ${OPTION_DEFINES}
        )
    endif()

    if(OPTION_FRAGMENT_TARGET AND OPTION_FRAGMENT_ENTRY)
        add_shader_compile(${SHADER_SOURCE}
            EMBED_TARGET ${OPTION_FRAGMENT_TARGET}
            ENTRY_POINT ${OPTION_FRAGMENT_ENTRY}
            STAGE fragment
            SLANGC_FLAGS ${OPTION_SLANGC_FLAGS}
            INCLUDE_DIRECTORIES ${OPTION_INCLUDE_DIRECTORIES}
            DEFINES ${OPTION_DEFINES}
        )
    endif()

    if(OPTION_COMPUTE_TARGET AND OPTION_COMPUTE_ENTRY)
        add_shader_compile(${SHADER_SOURCE}
            EMBED_TARGET ${OPTION_COMPUTE_TARGET}
            ENTRY_POINT ${OPTION_COMPUTE_ENTRY}
            STAGE compute
            SLANGC_FLAGS ${OPTION_SLANGC_FLAGS}
            INCLUDE_DIRECTORIES ${OPTION_INCLUDE_DIRECTORIES}
            DEFINES ${OPTION_DEFINES}
        )
    endif()

endfunction()