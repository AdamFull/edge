target_sources(${PROJECT_NAME} PRIVATE 
        "filesystem.cpp"
    )

if(WIN32)
target_sources(${PROJECT_NAME} PRIVATE "filesystem_win32.cpp")
elseif(ANDROID)
target_sources(${PROJECT_NAME} PRIVATE "filesystem_unix.cpp")
endif()