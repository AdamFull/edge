cmake_minimum_required(VERSION 3.22.1)
project(edge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(APP_SOURCES main.cpp)

if(ANDROID)
    find_package(game-activity REQUIRED CONFIG)

    add_library(${PROJECT_NAME} SHARED ${APP_SOURCES})

    target_link_libraries(${PROJECT_NAME} 
        PUBLIC
        log android 
        game-activity::game-activity_static
    )
elseif(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES})
elseif(APPLE)
    enable_language(OBJC OBJCXX)

    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(PLATFORM_APPLE_SOURCES
            "edge/platform/ios/main.m"
            "edge/platform/ios/AppDelegate.h"
            "edge/platform/ios/AppDelegate.m" 
            "edge/platform/ios/GameViewController.h"
            "edge/platform/ios/GameViewController.m"
        )
        set(PLATFORM_APPLE_RESOURCES
            "edge/platform/ios/Assets.xcassets"
            "edge/platform/ios/Main.storyboard"
            "edge/platform/ios/LaunchScreen.storyboard"
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(PLATFORM_SOURCES
            "edge/platform/osx/main.m"
            "edge/platform/osx/AppDelegate.h"
            "edge/platform/osx/AppDelegate.m"
            "edge/platform/osx/GameViewController.h" 
            "edge/platform/osx/GameViewController.m"
        )
        set(PLATFORM_RESOURCES
            "edge/platform/osx/Assets.xcassets"
            "edge/platform/osx/Main.storyboard"
        )
    endif()

    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        ${APP_SOURCES}
        ${PLATFORM_APPLE_SOURCES}
        ${PLATFORM_APPLE_RESOURCES}
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.adamfull.edge"
        MACOSX_BUNDLE_BUNDLE_NAME "edge"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.adamfull.edge"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "RXRSMFQFDD"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_GLOBAL_ACCENT_COLOR_NAME "AccentColor"
        XCODE_ATTRIBUTE_CLANG_ANALYZER_NONNULL YES
        XCODE_ATTRIBUTE_CLANG_ANALYZER_NUMBER_OBJECT_CONVERSION YES_AGGRESSIVE
        XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "gnu++23"
        XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES
        XCODE_ATTRIBUTE_GCC_C_LANGUAGE_STANDARD "gnu17"
        XCODE_ATTRIBUTE_LOCALIZATION_PREFERS_STRING_CATALOGS YES
        XCODE_ATTRIBUTE_MTL_FAST_MATH YES
        XCODE_ATTRIBUTE_ENABLE_USER_SCRIPT_SANDBOXING YES
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_GENERATE_SWIFT_ASSET_SYMBOL_EXTENSIONS YES
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.6"
            XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_UIApplicationSupportsIndirectInputEvents YES
            XCODE_ATTRIBUTE_INFOPLIST_KEY_UILaunchStoryboardName "LaunchScreen"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_UIMainStoryboardFile "Main"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_UIStatusBarHidden YES
            XCODE_ATTRIBUTE_INFOPLIST_KEY_UISupportedInterfaceOrientations_iPad "UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_UISupportedInterfaceOrientations_iPhone "UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight"
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework UIKit"
            "-framework Metal"
            "-framework MetalKit" 
            "-framework Foundation"
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "11.5"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHumanReadableCopyright ""
            XCODE_ATTRIBUTE_INFOPLIST_KEY_NSMainStoryboardFile "Main"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_NSPrincipalClass "NSApplication"
            XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES YES
            XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
            XCODE_ATTRIBUTE_REGISTER_APP_GROUPS YES
        )

        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework Cocoa"
            "-framework Metal"
            "-framework MetalKit"
            "-framework Foundation"
        )
    endif()

    foreach(RESOURCE ${PLATFORM_APPLE_RESOURCES})
        if(${RESOURCE} MATCHES "\\.storyboard$")
            set_source_files_properties(${RESOURCE} PROPERTIES
                MACOSX_PACKAGE_LOCATION Resources
            )
        elseif(${RESOURCE} MATCHES "\\.xcassets$")
            set_source_files_properties(${RESOURCE} PROPERTIES
                MACOSX_PACKAGE_LOCATION Resources
            )
        endif()
    endforeach()

    target_compile_options(${PROJECT_NAME} PRIVATE
        -fobjc-arc
        -ffast-math
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG=1)
        target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH YES
            XCODE_ATTRIBUTE_ENABLE_TESTABILITY YES
            XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL 0
            XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO "INCLUDE_SOURCE"
        )
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -O3 -DNDEBUG)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL 3
            XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO NO
            XCODE_ATTRIBUTE_VALIDATE_PRODUCT YES
        )
    endif()
else()
    add_executable(${PROJECT_NAME} ${APP_SOURCES})
endif()

add_subdirectory("edge/core/platform")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)