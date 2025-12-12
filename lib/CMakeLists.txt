add_subdirectory("base")
add_subdirectory("coro")
#add_subdirectory("http")
add_subdirectory("json")
add_subdirectory("logger")
add_subdirectory("tar")
add_subdirectory("zip")

add_library(edge_foundation INTERFACE)
target_link_libraries(edge_foundation
	INTERFACE
	edge_base
	edge_coro
	#edge_http_static
	edge_json
	edge_logger
	edge_tar
	edge_zip
)