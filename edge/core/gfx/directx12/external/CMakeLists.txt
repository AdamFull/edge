add_subdirectory("dxma")

# Pix
set(PIX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pix)

add_library(PixEvents SHARED IMPORTED)
set_target_properties(PixEvents PROPERTIES
    IMPORTED_IMPLIB "${PIX_ROOT_DIR}/lib/WinPixEventRuntime.lib"
    IMPORTED_LOCATION "${PIX_ROOT_DIR}/bin/WinPixEventRuntime.dll"
)
set_target_properties(PixEvents PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PIX_ROOT_DIR}/include"
)

add_library(d3d_externals INTERFACE)
target_link_libraries(d3d_externals
    INTERFACE
    D3D12MemoryAllocator
    PixEvents
)