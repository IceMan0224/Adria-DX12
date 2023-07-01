#include "DDGI.h"
#include "BlackboardData.h"
#include "ShaderCache.h"
#include "PSOCache.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Utilities/Random.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	XMUINT2 DDGI::ProbeTextureDimensions(XMUINT3 const& num_probes, uint32 texels_per_probe)
	{
		uint32 width = (1 + texels_per_probe + 1) * num_probes.y * num_probes.x;
		uint32 height = (1 + texels_per_probe + 1) * num_probes.z;
		return XMUINT2(width, height);
	}

	DDGI::DDGI(GfxDevice* gfx, entt::registry& reg, uint32 w, uint32 h) : gfx(gfx), reg(reg), width(w), height(h)
	{
		is_supported = gfx->GetCapabilities().SupportsRayTracing();
		if (IsSupported())
		{
			CreateStateObject();
			ShaderCache::GetLibraryRecompiledEvent().AddMember(&DDGI::OnLibraryRecompiled, *this);
		}
	}

	void DDGI::OnResize(uint32 w, uint32 h)
	{
		if (!IsSupported()) return;
		width = w, height = h;
	}

	void DDGI::AddPasses(RenderGraph& rg)
	{
		if (!IsSupported()) return;

		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();

		uint32 const num_probes = ddgi_volume.num_probes.x * ddgi_volume.num_probes.y * ddgi_volume.num_probes.z;
		uint32 const ddgi_num_rays = ddgi_volume.num_rays;
		uint32 const ddgi_max_num_rays = ddgi_volume.max_num_rays;
		XMUINT3 const ddgi_num_probes = ddgi_volume.num_probes;

		struct DDGIParameters
		{
			XMFLOAT3 random_vector;
			float    random_angle;
			float    history_blend_weight;
			uint32   ray_buffer_index;
		} parameters
		{
			.random_vector = XMFLOAT3(),
			.random_angle = 0.0f,
			.history_blend_weight = 0.98f,
			.ray_buffer_index = 0 //#todo
		};


		rg.ImportTexture(RG_RES_NAME(DDGIIrradianceHistory), ddgi_volume.irradiance_history.get());
		rg.ImportTexture(RG_RES_NAME(DDGIDepthHistory), ddgi_volume.depth_history.get());

		struct DDGIRayTracePassData
		{
			RGBufferReadWriteId ray_buffer;
		};

		rg.AddPass<DDGIRayTracePassData>("DDGI Ray Trace Pass",
			[=](DDGIRayTracePassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc ray_buffer_desc{};
				ray_buffer_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				ray_buffer_desc.stride = GetGfxFormatStride(ray_buffer_desc.format);
				ray_buffer_desc.size = ray_buffer_desc.stride * num_probes * ddgi_max_num_rays;
				builder.DeclareBuffer(RG_RES_NAME(DDGIRayBuffer), ray_buffer_desc);
				data.ray_buffer	  = builder.WriteBuffer(RG_RES_NAME(DDGIRayBuffer));
			},
			[=](DDGIRayTracePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				uint32 i = gfx->AllocateDescriptorsGPU(1).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i + 0), ctx.GetReadWriteBuffer(data.ray_buffer));

				auto& table = cmd_list->SetStateObject(ddgi_trace_so.Get());
				table.SetRayGenShader("DDGI_RayGen");
				table.AddMissShader("DDGI_Miss", 0);
				table.AddHitGroup("DDGI_ClosestHit", 0);

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->SetRootCBV(2, ddgi_volume);
				cmd_list->DispatchRays(ddgi_num_rays, num_probes);
				cmd_list->UavBarrier(ctx.GetBuffer(*data.ray_buffer));

			}, RGPassType::Compute);

		struct DDGIUpdateIrradiancePassData
		{
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		irradiance_history;
			RGTextureReadWriteId	irradiance;
		};

		rg.AddPass<DDGIUpdateIrradiancePassData>("DDGI Update Irradiance Pass",
			[=](DDGIUpdateIrradiancePassData& data, RenderGraphBuilder& builder)
			{
				XMUINT2 irradiance_dimensions = ProbeTextureDimensions(ddgi_num_probes, PROBE_IRRADIANCE_TEXELS);
				RGTextureDesc irradiance_desc{};
				irradiance_desc.width  = irradiance_dimensions.x;
				irradiance_desc.height = irradiance_dimensions.y;
				irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(DDGIIrradiance), irradiance_desc);

				data.irradiance		= builder.WriteTexture(RG_RES_NAME(DDGIIrradiance));
				data.ray_buffer		= builder.ReadBuffer(RG_RES_NAME(DDGIRayBuffer));
				data.irradiance_history = builder.ReadTexture(RG_RES_NAME(DDGIIrradianceHistory));
			},
			[=](DDGIUpdateIrradiancePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateIrradiance));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->Dispatch(num_probes, 1, 1);
				cmd_list->UavBarrier(ctx.GetTexture(*data.irradiance));
			}, RGPassType::Compute);

		struct DDGIUpdateDistancePassData
		{
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		depth_history;
			RGTextureReadWriteId	depth;
		};

		rg.AddPass<DDGIUpdateDistancePassData>("DDGI Update Distance Pass",
			[=](DDGIUpdateDistancePassData& data, RenderGraphBuilder& builder)
			{
				XMUINT2 depth_dimensions = ProbeTextureDimensions(ddgi_num_probes, PROBE_DEPTH_TEXELS);
				RGTextureDesc irradiance_desc{};
				irradiance_desc.width = depth_dimensions.x;
				irradiance_desc.height = depth_dimensions.y;
				irradiance_desc.format = GfxFormat::R16G16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(DDGIDistance), irradiance_desc);

				data.depth = builder.WriteTexture(RG_RES_NAME(DDGIDistance));
				data.ray_buffer = builder.ReadBuffer(RG_RES_NAME(DDGIRayBuffer));
				data.depth_history = builder.ReadTexture(RG_RES_NAME(DDGIDistanceHistory));
			},
			[=](DDGIUpdateDistancePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateDistance));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->Dispatch(num_probes, 1, 1);
				cmd_list->UavBarrier(ctx.GetTexture(*data.depth));
			}, RGPassType::Compute);

		struct DDGIUpdateIrradianceBorderPassData
		{
			RGTextureReadWriteId	irradiance;
		};

		rg.AddPass<DDGIUpdateIrradianceBorderPassData>("DDGI Update Irradiance Border Pass",
			[=](DDGIUpdateIrradianceBorderPassData& data, RenderGraphBuilder& builder)
			{
				data.irradiance = builder.WriteTexture(RG_RES_NAME(DDGIIrradiance));
			},
			[=](DDGIUpdateIrradianceBorderPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateIrradianceBorder));
				cmd_list->UavBarrier(ctx.GetTexture(*data.irradiance));
			}, RGPassType::Compute);

		struct DDGIUpdateDistanceBorderPassData
		{
			RGTextureReadWriteId	distance;
		};

		rg.AddPass<DDGIUpdateDistanceBorderPassData>("DDGI Update Distance Border Pass",
			[=](DDGIUpdateDistanceBorderPassData& data, RenderGraphBuilder& builder)
			{
				data.distance = builder.WriteTexture(RG_RES_NAME(DDGIDistance));
			},
			[=](DDGIUpdateDistanceBorderPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateDistanceBorder));
				cmd_list->UavBarrier(ctx.GetTexture(*data.distance));
			}, RGPassType::Compute);
		
		rg.ExportTexture(RG_RES_NAME(DDGIIradiance), ddgi_volume.irradiance_history.get());
		rg.ExportTexture(RG_RES_NAME(DDGIDepth), ddgi_volume.depth_history.get());
	}

	void DDGI::CreateStateObject()
	{

	}

	void DDGI::OnLibraryRecompiled(GfxShaderID shader)
	{
		//if (shader == LIB_DDGIRayTracing) CreateStateObject();
	}
}

