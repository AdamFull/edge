include(ShaderCompiler)

add_shader_compile_multi(
    imgui.slang
    VERTEX_TARGET imgui_vs
    VERTEX_ENTRY vs_main
    FRAGMENT_TARGET imgui_fs
    FRAGMENT_ENTRY ps_main
    DEFINES
        __SLANG__=1
    SLANGC_FLAGS
        -fvk-use-dx-layout
        -fvk-use-scalar-layout
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    imgui_vs
    imgui_fs
)