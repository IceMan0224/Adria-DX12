#include "PSOCache.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxStateObject.h"
#include "Graphics/GfxStates.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxReflection.h"
#include "Logging/Logger.h"
#include "Utilities/Timer.h"

namespace adria
{
	namespace
	{
		GfxDevice* gfx;
		std::unordered_map<GfxPipelineStateID, std::unique_ptr<GraphicsPipelineState>>		gfx_pso_map;
		std::unordered_map<GfxPipelineStateID, std::unique_ptr<ComputePipelineState>>		compute_pso_map;
		std::unordered_map<GfxPipelineStateID, std::unique_ptr<MeshShaderPipelineState>>    mesh_pso_map;

		inline GfxShader const& GetShader(ShaderID shader)
		{
			return ShaderManager::GetShader(shader);
		}
		void CreateAllPSOs()
		{
			GraphicsPipelineStateDesc gfx_pso_desc{};
			{
				GfxReflection::FillInputLayoutDesc(GetShader(VS_Sky), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Sky;
				gfx_pso_desc.PS = PS_Sky;
				gfx_pso_desc.rasterizer_state.cull_mode = GfxCullMode::None;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Sky] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc = {};
				GfxReflection::FillInputLayoutDesc(GetShader(VS_Sun), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Sun;
				gfx_pso_desc.PS = PS_Texture;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::SrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::InvSrcAlpha;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::Zero;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Sun] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_LensFlare;
				gfx_pso_desc.GS = GS_LensFlare;
				gfx_pso_desc.PS = PS_LensFlare;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;

				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Point;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;

				gfx_pso_map[GfxPipelineStateID::LensFlare] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc = {};
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Bokeh;
				gfx_pso_desc.GS = GS_Bokeh;
				gfx_pso_desc.PS = PS_Bokeh;
				gfx_pso_desc.blend_state.render_target[0].blend_enable = true;
				gfx_pso_desc.blend_state.render_target[0].src_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].dest_blend = GfxBlend::One;
				gfx_pso_desc.blend_state.render_target[0].blend_op = GfxBlendOp::Add;

				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Point;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Bokeh] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc = {};
				GfxReflection::FillInputLayoutDesc(GetShader(VS_Ocean), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Ocean;
				gfx_pso_desc.PS = PS_Ocean;
				gfx_pso_desc.depth_state.depth_enable = true;
				gfx_pso_desc.depth_state.depth_write_mask = GfxDepthWriteMask::All;
				gfx_pso_desc.depth_state.depth_func = GfxComparisonFunc::GreaterEqual;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.dsv_format = GfxFormat::D32_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Ocean] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Wireframe;
				gfx_pso_map[GfxPipelineStateID::Ocean_Wireframe] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_OceanLOD;
				gfx_pso_desc.DS = DS_OceanLOD;
				gfx_pso_desc.HS = HS_OceanLOD;
				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Patch;
				gfx_pso_map[GfxPipelineStateID::OceanLOD_Wireframe] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Solid;
				gfx_pso_map[GfxPipelineStateID::OceanLOD] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc = {};
				GfxReflection::FillInputLayoutDesc(GetShader(VS_Simple), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Simple;
				gfx_pso_desc.PS = PS_Texture;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_map[GfxPipelineStateID::Texture] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);

				gfx_pso_desc = {};
				GfxReflection::FillInputLayoutDesc(GetShader(VS_Simple), gfx_pso_desc.input_layout);
				gfx_pso_desc.root_signature = GfxRootSignatureID::Common;
				gfx_pso_desc.VS = VS_Simple;
				gfx_pso_desc.PS = PS_Solid;
				gfx_pso_desc.num_render_targets = 1;
				gfx_pso_desc.rtv_formats[0] = GfxFormat::R16G16B16A16_FLOAT;
				gfx_pso_desc.depth_state.depth_enable = false;
				gfx_pso_desc.rasterizer_state.fill_mode = GfxFillMode::Wireframe;
				gfx_pso_desc.topology_type = GfxPrimitiveTopologyType::Line;
				gfx_pso_map[GfxPipelineStateID::Solid_Wireframe] = gfx->CreateGraphicsPipelineState(gfx_pso_desc);


			}

			ComputePipelineStateDesc compute_pso_desc{};
			compute_pso_desc.root_signature = GfxRootSignatureID::Common;
			{
				compute_pso_desc.CS = CS_ClusteredDeferredLighting;
				compute_pso_map[GfxPipelineStateID::ClusteredDeferredLighting] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_ClusterBuilding;
				compute_pso_map[GfxPipelineStateID::ClusterBuilding] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_ClusterCulling;
				compute_pso_map[GfxPipelineStateID::ClusterCulling] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_TiledDeferredLighting;
				compute_pso_map[GfxPipelineStateID::TiledDeferredLighting] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Picking;
				compute_pso_map[GfxPipelineStateID::Picking] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Blur_Horizontal;
				compute_pso_map[GfxPipelineStateID::Blur_Horizontal] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Blur_Vertical;
				compute_pso_map[GfxPipelineStateID::Blur_Vertical] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_GenerateMips;
				compute_pso_map[GfxPipelineStateID::GenerateMips] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_BokehGeneration;
				compute_pso_map[GfxPipelineStateID::BokehGenerate] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_FFT_Horizontal;
				compute_pso_map[GfxPipelineStateID::FFT_Horizontal] = gfx->CreateComputePipelineState(compute_pso_desc);
				compute_pso_desc.CS = CS_FFT_Vertical;
				compute_pso_map[GfxPipelineStateID::FFT_Vertical] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_InitialSpectrum;
				compute_pso_map[GfxPipelineStateID::InitialSpectrum] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_OceanNormals;
				compute_pso_map[GfxPipelineStateID::OceanNormals] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Phase;
				compute_pso_map[GfxPipelineStateID::Phase] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Spectrum;
				compute_pso_map[GfxPipelineStateID::Spectrum] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_BuildHistogram;
				compute_pso_map[GfxPipelineStateID::BuildHistogram] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_HistogramReduction;
				compute_pso_map[GfxPipelineStateID::HistogramReduction] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Exposure;
				compute_pso_map[GfxPipelineStateID::Exposure] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Ssao;
				compute_pso_map[GfxPipelineStateID::SSAO] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Hbao;
				compute_pso_map[GfxPipelineStateID::HBAO] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Ssr;
				compute_pso_map[GfxPipelineStateID::SSR] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_ExponentialHeightFog;
				compute_pso_map[GfxPipelineStateID::ExponentialHeightFog] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Tonemap;
				compute_pso_map[GfxPipelineStateID::ToneMap] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_MotionVectors;
				compute_pso_map[GfxPipelineStateID::MotionVectors] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_MotionBlur;
				compute_pso_map[GfxPipelineStateID::MotionBlur] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Dof;
				compute_pso_map[GfxPipelineStateID::DOF] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_GodRays;
				compute_pso_map[GfxPipelineStateID::GodRays] = gfx->CreateComputePipelineState(compute_pso_desc);
				
				compute_pso_desc.CS = CS_FilmEffects;
				compute_pso_map[GfxPipelineStateID::FilmEffects] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Fxaa;
				compute_pso_map[GfxPipelineStateID::FXAA] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Ambient;
				compute_pso_map[GfxPipelineStateID::Ambient] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Taa;
				compute_pso_map[GfxPipelineStateID::TAA] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_DeferredLighting;
				compute_pso_map[GfxPipelineStateID::DeferredLighting] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_VolumetricLighting;
				compute_pso_map[GfxPipelineStateID::VolumetricLighting] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_MinimalAtmosphereSky;
				compute_pso_map[GfxPipelineStateID::MinimalAtmosphereSky] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_HosekWilkieSky;
				compute_pso_map[GfxPipelineStateID::HosekWilkieSky] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_LensFlare2;
				compute_pso_map[GfxPipelineStateID::LensFlare2] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_RTAOFilter;
				compute_pso_map[GfxPipelineStateID::RTAOFilter] = gfx->CreateComputePipelineState(compute_pso_desc);

				if (gfx->GetCapabilities().CheckRayTracingSupport(RayTracingSupport::Tier1_1))
				{
					compute_pso_desc.CS = CS_ReSTIRGI_InitialSampling;
					compute_pso_map[GfxPipelineStateID::ReSTIRGI_InitialSampling] = gfx->CreateComputePipelineState(compute_pso_desc);
				}
			}
		}
	}

	void PSOCache::Initialize(GfxDevice* _gfx)
	{
		gfx = _gfx;
		CreateAllPSOs();
	}

	void PSOCache::Destroy()
	{
		gfx_pso_map.clear();
		compute_pso_map.clear();
		mesh_pso_map.clear();
	}

	GfxPipelineState* PSOCache::Get(GfxPipelineStateID ps)
	{
		if (compute_pso_map.contains(ps)) return compute_pso_map[ps].get();
		else if (mesh_pso_map.contains(ps)) return mesh_pso_map[ps].get();
		else return gfx_pso_map[ps].get();
	}
}

