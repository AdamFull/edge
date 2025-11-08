add_executable(example_basic example_basic.c)
add_executable(example_extract example_extract.c)

if(EDGE_ZIP_BUILD_SHARED)
	target_link_libraries(example_basic edge_zip)
	target_link_libraries(example_extract edge_zip)
endif()

if(EDGE_ZIP_BUILD_STATIC)
	target_link_libraries(example_basic edge_zip_static)
	target_link_libraries(example_extract edge_zip_static)
endif()