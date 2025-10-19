target_sources(${PROJECT_NAME} PRIVATE 
        "filesystem.cpp"
    )

if(WIN32)
# Windows-specific sources
target_sources(${PROJECT_NAME} PRIVATE 
    "filesystem_win32.cpp"
)
endif()