add_executable(zip_example_basic example_basic.c)
add_executable(zip_example_extract example_extract.c)

if(EDGE_ZIP_BUILD_SHARED)
	target_link_libraries(zip_example_basic edge_zip)
	target_link_libraries(zip_example_extract edge_zip)
endif()

if(EDGE_ZIP_BUILD_STATIC)
	target_link_libraries(zip_example_basic edge_zip_static)
	target_link_libraries(zip_example_extract edge_zip_static)
endif()