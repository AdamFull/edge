cmake_minimum_required(VERSION 3.12)

# Test executable
add_executable(edge_json_test test_edge_json.cpp)

# Include directories
target_include_directories(edge_json_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(edge_json_test PRIVATE edge_json edge_base)

# Compiler flags
target_compile_options(edge_json_test PRIVATE -Wall)

# Add test
add_test(NAME edge_json_test COMMAND edge_json_test)

# Valgrind target
find_program(VALGRIND valgrind)
if(VALGRIND)
    add_custom_target(valgrind
        COMMAND ${VALGRIND} --leak-check=full --show-leak-kinds=all --error-exitcode=1 $<TARGET_FILE:edge_json_test>
        DEPENDS edge_json_test
    )
endif()
