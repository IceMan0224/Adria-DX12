#include "GpuDebugFeature.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{
	GpuDebugFeature::GpuDebugFeature(GfxDevice* gfx, RGResourceName gpu_buffer_name) : gfx(gfx), gpu_buffer_name(gpu_buffer_name)
	{
		GfxBufferDesc gpu_buffer_desc{};
		gpu_buffer_desc.stride = sizeof(Uint32);
		gpu_buffer_desc.resource_usage = GfxResourceUsage::Default;
		gpu_buffer_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		gpu_buffer_desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		gpu_buffer_desc.size = gpu_buffer_desc.stride * 1024 * 1024;
		gpu_buffer = gfx->CreateBuffer(gpu_buffer_desc); 

		srv_descriptor = gfx->CreateBufferSRV(gpu_buffer.get());
		uav_descriptor = gfx->CreateBufferUAV(gpu_buffer.get());
		gfx->GetGraphicsCommandList()->BufferBarrier(*gpu_buffer, GfxResourceState::Common, GfxResourceState::ComputeUAV);

		for (auto& readback_buffer : cpu_readback_buffers)
		{
			readback_buffer = gfx->CreateBuffer(ReadBackBufferDesc(gpu_buffer_desc.size));
		}
	}
	Int32 GpuDebugFeature::GetBufferIndex()
	{
		return gfx->GetBindlessDescriptorIndex(uav_descriptor);
	}

	void GpuDebugFeature::AddClearPass(RenderGraph& rg, Char const* pass_name)
	{
		rg.ImportBuffer(gpu_buffer_name, gpu_buffer.get());

		struct ClearBufferPassData
		{
			RGBufferReadWriteId printf_buffer;
		};

		rg.AddPass<ClearBufferPassData>(pass_name,
			[=, this](ClearBufferPassData& data, RenderGraphBuilder& builder)
			{
				data.printf_buffer = builder.WriteBuffer(gpu_buffer_name);
			},
			[=, this](ClearBufferPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();
				Uint32 clear[] = { 0, 0, 0, 0 };
				cmd_list->ClearBuffer(*gpu_buffer, clear);
			}, RGPassType::Compute, RGPassFlags::ForceNoCull);
	}

	void GpuDebugFeature::AddFeaturePass(RenderGraph& rg, Char const* pass_name)
	{
		struct CopyBufferPassData
		{
			RGBufferCopySrcId gpu_buffer;
		};
		rg.AddPass<CopyBufferPassData>(pass_name,
			[=, this](CopyBufferPassData& data, RenderGraphBuilder& builder)
			{
				data.gpu_buffer = builder.ReadCopySrcBuffer(gpu_buffer_name);
				std::ignore = builder.ReadCopySrcTexture(RG_NAME(FinalTexture)); //forcing dependency with the final texture so the debug pass doesn't run before some other pass
			},
			[&](CopyBufferPassData const& data, RenderGraphContext& ctx)
			{
				GfxCommandList* cmd_list = ctx.GetCommandList();
				GfxDevice* gfx = cmd_list->GetDevice();
				Uint64 current_backbuffer_index = gfx->GetBackbufferIndex();
				GfxBuffer& readback_buffer = *cpu_readback_buffers[current_backbuffer_index];
				cmd_list->CopyBuffer(readback_buffer, *gpu_buffer);
				Uint64 old_backbuffer_index = (current_backbuffer_index + 1) % gfx->GetBackbufferCount();
				GfxBuffer& old_readback_buffer = *cpu_readback_buffers[old_backbuffer_index];
				ProcessBufferData(old_readback_buffer);
			}, RGPassType::Copy, RGPassFlags::ForceNoCull);
	}
	GpuDebugFeature::~GpuDebugFeature() = default;
}

