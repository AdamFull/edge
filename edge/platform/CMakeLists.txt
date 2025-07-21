if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        "windows_context.cpp"
    )
elseif(ANDROID)
endif()