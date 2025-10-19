target_sources(${PROJECT_NAME} PRIVATE 
	"application.cpp"
)

add_subdirectory("filesystem")
add_subdirectory("gfx")
add_subdirectory("platform")