# Examples CMakeLists.txt

# Basic example
add_executable(example example.c)
target_link_libraries(example PRIVATE edge_http ${CURL_LIBRARIES})
target_include_directories(example PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Installation
install(TARGETS example
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/edge-http-examples
)

if(TARGET custom_allocator)
    install(TARGETS custom_allocator
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/edge-http-examples
    )
endif()
