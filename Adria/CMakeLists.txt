
#for now
file(GLOB_RECURSE ADRIA_SOURCES
    "*.h"
    "*.cpp"
    "*.rc"
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ADRIA_SOURCES})

set(EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/External")

set(EXTERNAL_SOURCES
    "${EXTERNAL_DIR}/ImGui/imgui.cpp"
    "${EXTERNAL_DIR}/ImGui/imgui_draw.cpp"
    "${EXTERNAL_DIR}/ImGui/imgui_tables.cpp"
    "${EXTERNAL_DIR}/ImGui/imgui_widgets.cpp"
    "${EXTERNAL_DIR}/ImGui/imgui_impl_dx12.cpp"
    "${EXTERNAL_DIR}/ImGui/imgui_impl_win32.cpp"
	
	"${EXTERNAL_DIR}/SimpleMath/SimpleMath.cpp"
	
	"${EXTERNAL_DIR}/D3D12MA/D3D12MemAlloc.cpp"
	
	"${EXTERNAL_DIR}/ImPlot/implot.cpp"
	"${EXTERNAL_DIR}/ImPlot/implot_items.cpp"
	
	"${EXTERNAL_DIR}/meshoptimizer/allocator.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/clusterizer.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/indexcodec.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/indexgenerator.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/overdrawanalyzer.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/overdrawoptimizer.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/partition.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/quantization.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/simplifier.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/spatialorder.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/stripifier.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/vcacheanalyzer.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/vcacheoptimizer.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/vfetchanalyzer.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/vfetchoptimizer.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/vertexcodec.cpp"
    "${EXTERNAL_DIR}/meshoptimizer/vertexfilter.cpp"
)

add_executable(Adria WIN32 ${ADRIA_SOURCES} ${EXTERNAL_SOURCES})

target_precompile_headers(Adria PRIVATE precomp.h)

target_include_directories(Adria PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${EXTERNAL_DIR}/d3dx12"
    "${EXTERNAL_DIR}/DirectMLX"
    "${EXTERNAL_DIR}/D3D12MA"
    "${EXTERNAL_DIR}/stb"
    "${EXTERNAL_DIR}/dxc/include"
    "${EXTERNAL_DIR}/cgltf"
    "${EXTERNAL_DIR}/tinyobjloader"
    "${EXTERNAL_DIR}/FastNoiseLite"
    "${EXTERNAL_DIR}/json"
    "${EXTERNAL_DIR}/entt"
    "${EXTERNAL_DIR}/nfd/include"
    "${EXTERNAL_DIR}/FontAwesome"
    "${EXTERNAL_DIR}/meshoptimizer"
    "${EXTERNAL_DIR}/cereal"
    "${EXTERNAL_DIR}/ImGui"
    "${EXTERNAL_DIR}/Tracy"
    "${EXTERNAL_DIR}/SimpleMath"
    "${EXTERNAL_DIR}/XeSS/inc"
    "${EXTERNAL_DIR}/DLSS/include"
    "${EXTERNAL_DIR}/FidelityFX-SDK/include"
    "${EXTERNAL_DIR}/NVIDIA_Aftermath_SDK/include"
    "${EXTERNAL_DIR}/NvPerf"
    "${EXTERNAL_DIR}/ImPlot"
    "${EXTERNAL_DIR}/oidn-2.3.2/include"
    "${EXTERNAL_DIR}/renderdoc"
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

target_link_libraries(Adria PRIVATE
    Microsoft::DirectX-Headers
    Microsoft::WinPixEventRuntime
    d3d12.lib
    dxgi.lib
    dxguid.lib
    DirectML.lib
    "${EXTERNAL_DIR}/NVIDIA_Aftermath_SDK/lib/GFSDK_Aftermath_Lib.x64.lib"
    "${EXTERNAL_DIR}/XeSS/lib/libxess.lib"

    $<$<CONFIG:Debug>:
    "${EXTERNAL_DIR}/nfd/lib/nfd_d.lib"
    "${EXTERNAL_DIR}/DLSS/lib/nvsdk_ngx_d_dbg.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_dof_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_fsr2_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_fsr3_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_cas_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_backend_dx12_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_cacao_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_frameinterpolation_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_opticalflow_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_fsr3upscaler_x64d.lib"
    "${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_vrs_x64d.lib"
	>

	$<$<CONFIG:Profile,Release,RelWithDebInfo>:
		"${EXTERNAL_DIR}/nfd/lib/nfd.lib"
		"${EXTERNAL_DIR}/DLSS/lib/nvsdk_ngx_d.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_dof_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_fsr2_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_fsr3_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_cas_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_backend_dx12_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_cacao_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_frameinterpolation_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_opticalflow_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_fsr3upscaler_x64.lib"
		"${EXTERNAL_DIR}/FidelityFX-SDK/lib/ffx_vrs_x64.lib"
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
copy_runtime_dll("GFSDK_Aftermath_Lib.x64.dll" Adria)
copy_runtime_dll("libxess.dll" Adria)
copy_runtime_dll("nvperf_grfx_host.dll" Adria)
copy_runtime_dll("renderdoc.dll" Adria)
copy_runtime_dll("WinPixEventRuntime.dll" Adria)