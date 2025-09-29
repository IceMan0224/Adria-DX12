
#for now
file(GLOB_RECURSE ADRIA_SOURCES
    "*.h"
    "*.cpp"
    "*.rc"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ADRIA_SOURCES})

add_executable(Adria ${ADRIA_SOURCES})
set_target_properties(Adria PROPERTIES VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0") 

target_precompile_headers(Adria PRIVATE precomp.h)

target_include_directories(Adria PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/External/d3dx12"
    "${CMAKE_SOURCE_DIR}/External/DirectMLX"
    "${CMAKE_SOURCE_DIR}/External/D3D12MA"
    "${CMAKE_SOURCE_DIR}/External/stb"
    "${CMAKE_SOURCE_DIR}/External/dxc/include"
    "${CMAKE_SOURCE_DIR}/External/cgltf"
    "${CMAKE_SOURCE_DIR}/External/tinyobjloader"
    "${CMAKE_SOURCE_DIR}/External/FastNoiseLite"
    "${CMAKE_SOURCE_DIR}/External/json"
    "${CMAKE_SOURCE_DIR}/External/entt"
    "${CMAKE_SOURCE_DIR}/External/nfd/include"
    "${CMAKE_SOURCE_DIR}/External/FontAwesome"
    "${CMAKE_SOURCE_DIR}/External/meshoptimizer"
    "${CMAKE_SOURCE_DIR}/External/cereal"
    "${CMAKE_SOURCE_DIR}/External/ImGui"
    "${CMAKE_SOURCE_DIR}/External/Tracy"
    "${CMAKE_SOURCE_DIR}/External/SimpleMath"
    "${CMAKE_SOURCE_DIR}/External/XeSS/inc"
    "${CMAKE_SOURCE_DIR}/External/DLSS/include"
    "${CMAKE_SOURCE_DIR}/External/FidelityFX-SDK/include"
    "${CMAKE_SOURCE_DIR}/External/NVIDIA_Aftermath_SDK/include"
    "${CMAKE_SOURCE_DIR}/External/NvPerf"
    "${CMAKE_SOURCE_DIR}/External/ImPlot"
    "${CMAKE_SOURCE_DIR}/External/oidn-2.3.2/include"
    "${CMAKE_SOURCE_DIR}/External/renderdoc"
)

target_compile_options(Adria PRIVATE /MP)

target_compile_definitions(Adria PRIVATE
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
    _SILENCE_CXX20_CISO646_REMOVED_WARNING
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    "SOLUTION_DIR=R\"(${CMAKE_SOURCE_DIR})\""
    "SOLUTION_DIR_W=LR\"(${CMAKE_SOURCE_DIR})\""
)

target_compile_definitions(Adria PRIVATE
    $<$<CONFIG:Debug>:_DEBUG;_CONSOLE>
    $<$<CONFIG:Profile>:NDEBUG;_PROFILE;_CONSOLE;TRACY_ENABLE>
    $<$<CONFIG:Release,RelWithDebInfo>:NDEBUG;_CONSOLE>
)

link_directories(
    "${CMAKE_SOURCE_DIR}/External/nfd/lib"
    "${CMAKE_SOURCE_DIR}/External/XeSS/lib"
    "${CMAKE_SOURCE_DIR}/External/DLSS/lib"
    "${CMAKE_SOURCE_DIR}/External/FidelityFX-SDK/lib"
    "${CMAKE_SOURCE_DIR}/External/NVIDIA_Aftermath_SDK/lib"
    "${CMAKE_SOURCE_DIR}/External/oidn-2.3.2/lib"
)

target_link_libraries(Adria PRIVATE
    Microsoft::DirectX-Headers
    Microsoft::WinPixEventRuntime
    d3d12.lib
    dxgi.lib
    dxguid.lib
    DirectML.lib
    dxcompiler.lib
    GFSDK_Aftermath_Lib.lib
    OpenImageDenoise.lib
    OpenImageDenoise_core.lib
    libxess.lib

    $<$<CONFIG:Debug>:
        nfd_d.lib
        nvsdk_ngx_d_dbg.lib
        ffx_dof_x64d.lib;ffx_fsr2_x64d.lib;ffx_fsr3_x64d.lib;ffx_cas_x64d.lib;ffx_backend_dx12_x64d.lib;ffx_cacao_x64d.lib;ffx_frameinterpolation_x64d.lib;ffx_opticalflow_x64d.lib;ffx_fsr3upscaler_x64d.lib;ffx_vrs_x64d.lib
    >

    $<$<CONFIG:Profile, Release,RelWithDebInfo>:
        nfd.lib
        nvsdk_ngx_d.lib
        ffx_dof_x64.lib;ffx_fsr2_x64.lib;ffx_fsr3_x64.lib;ffx_cas_x64.lib;ffx_backend_dx12_x64.lib;ffx_cacao_x64.lib;ffx_frameinterpolation_x64.lib;ffx_opticalflow_x64.lib;ffx_fsr3upscaler_x64.lib;ffx_vrs_x64.lib
    >
)

function(copy_runtime_dll SOURCE_FILE TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}"
        $<TARGET_FILE_DIR:${TARGET_NAME}>
        COMMENT "Copying ${SOURCE_FILE} to output directory"
    )
endfunction()

copy_runtime_dll("dxcompiler.dll" Adria)
copy_runtime_dll("dxil.dll" Adria)
copy_runtime_dll("GFSDK_Aftermath_Lib.dll" Adria)
copy_runtime_dll("libxess.dll" Adria)
copy_runtime_dll("nvperf_grfx_host.dll" Adria)
copy_runtime_dll("renderdoc.dll" Adria)
copy_runtime_dll("WinPixEventRuntime.dll" Adria)