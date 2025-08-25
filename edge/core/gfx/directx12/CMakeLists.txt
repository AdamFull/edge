add_subdirectory("external")

set(ENABLE_D3D12_VALIDATION ON CACHE BOOL "Enable D3D12 validation.")
set(ENABLE_D3D12_DEBUG ON CACHE BOOL "Enable d3d debugging if possible.")

if(${ENABLE_D3D12_DEBUG})
    if(${ENABLE_D3D12_VALIDATION})
        target_compile_definitions(${PROJECT_NAME} PUBLIC D3D12_VALIDATION)
    endif()
endif(ENABLE_D3D12_DEBUG)

if(USE_NSIGHT_AFTERMATH)
    #target_link_libraries(${PROJECT_NAME} PRIVATE nsight_aftermath_interface)
endif()

find_library(D3D12_LIBRARY d3d12 REQUIRED)
find_library(DXGI_LIBRARY dxgi REQUIRED)
find_library(D3DCOMPILER_LIBRARY d3dcompiler REQUIRED)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE
    ${D3D12_LIBRARY}
    ${DXGI_LIBRARY}
    ${D3DCOMPILER_LIBRARY}
    d3d_externals
)

target_sources(${PROJECT_NAME} PRIVATE
	"dx12_context.cpp"
    "dx12_primitives.cpp"
)