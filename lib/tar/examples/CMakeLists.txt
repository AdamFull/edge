add_executable(tar_example_basic example_basic.c)
add_executable(tar_example_extract example_extract.c)

if(EDGE_TAR_BUILD_SHARED)
	target_link_libraries(tar_example_basic edge_tar)
	target_link_libraries(tar_example_extract edge_tar)
endif()

if(EDGE_TAR_BUILD_STATIC)
	target_link_libraries(tar_example_basic edge_tar_static)
	target_link_libraries(tar_example_extract edge_tar_static)
endif()