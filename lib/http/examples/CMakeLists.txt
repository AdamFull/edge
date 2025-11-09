add_executable(http_example_basic example_basic.c)

if(EDGE_HTTP_BUILD_SHARED)
	target_link_libraries(http_example_basic edge_http)
endif()

if(EDGE_HTTP_BUILD_STATIC)
	target_link_libraries(http_example_basic edge_http_static)
endif()