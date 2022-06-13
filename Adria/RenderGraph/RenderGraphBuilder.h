#pragma once
#include "RenderGraphResourceName.h"
#include "RenderGraphPass.h"
#include "RenderGraphContext.h"

namespace adria
{
	struct RGTextureDesc
	{
		ETextureType type = TextureType_2D;
		uint32 width = 0;
		uint32 height = 0;
		uint32 depth = 0;
		uint32 array_size = 1;
		uint32 mip_levels = 1;
		uint32 sample_count = 1;
		EResourceUsage heap_type = EResourceUsage::Default;
		ETextureMiscFlag misc_flags = ETextureMiscFlag::None;
		ClearValue clear_value{};
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	};
	struct RGBufferDesc
	{
		uint64 size = 0;
		uint32 stride = 0;
		EResourceUsage heap_type = EResourceUsage::Default;
		EBufferMiscFlag misc_flags = EBufferMiscFlag::None;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	};

	static void FillTextureDesc(RGTextureDesc const& rg_tex_desc, TextureDesc& tex_desc)
	{
		tex_desc.type = rg_tex_desc.type;
		tex_desc.width = rg_tex_desc.width;
		tex_desc.height = rg_tex_desc.height;
		tex_desc.depth = rg_tex_desc.depth;
		tex_desc.array_size = rg_tex_desc.array_size;
		tex_desc.mip_levels = rg_tex_desc.mip_levels;
		tex_desc.format = rg_tex_desc.format;
		tex_desc.sample_count = rg_tex_desc.sample_count;
		tex_desc.heap_type = rg_tex_desc.heap_type;
		tex_desc.misc_flags = rg_tex_desc.misc_flags;
		tex_desc.clear_value = rg_tex_desc.clear_value;
		tex_desc.bind_flags = EBindFlag::None;
		tex_desc.initial_state = EResourceState::Common;
	}
	static void FillBufferDesc(RGBufferDesc const& rg_buf_desc, BufferDesc& buf_desc)
	{
		buf_desc.size = rg_buf_desc.size;
		buf_desc.stride = rg_buf_desc.stride;
		buf_desc.heap_type = rg_buf_desc.heap_type;
		buf_desc.misc_flags = rg_buf_desc.misc_flags;
		buf_desc.format = rg_buf_desc.format;
		buf_desc.bind_flags = EBindFlag::None;
	}

	class RenderGraphBuilder
	{
		friend class RenderGraph;

	public:
		RenderGraphBuilder() = delete;
		RenderGraphBuilder(RenderGraphBuilder const&) = delete;
		RenderGraphBuilder& operator=(RenderGraphBuilder const&) = delete;

		bool IsTextureDeclared(RGResourceName name);
		void DeclareTexture(RGResourceName name, RGTextureDesc const& desc);
		void DeclareBuffer(RGResourceName name, RGBufferDesc const& desc);
		void DeclareAllocation(RGResourceName name, AllocDesc const& desc);

		void DummyWriteTexture(RGResourceName name);
		void DummyReadTexture(RGResourceName name);
		void DummyReadBuffer(RGResourceName name);
		void DummyWriteBuffer(RGResourceName name);

		[[nodiscard]] RGTextureCopySrcId ReadCopySrcTexture(RGResourceName name);
		[[nodiscard]] RGTextureCopyDstId WriteCopyDstTexture(RGResourceName name);
		[[nodiscard]] RGTextureReadOnlyId ReadTexture(RGResourceName name, ERGReadAccess read_access, TextureViewDesc const& desc = {});
		[[nodiscard]] RGTextureReadWriteId WriteTexture(RGResourceName name, TextureViewDesc const& desc = {});
		[[maybe_unused]] RGRenderTargetId WriteRenderTarget(RGResourceName name, ERGLoadStoreAccessOp load_store_op, TextureViewDesc const& desc = {});
		[[maybe_unused]] RGDepthStencilId WriteDepthStencil(RGResourceName name, ERGLoadStoreAccessOp load_store_op, ERGLoadStoreAccessOp stencil_load_store_op = ERGLoadStoreAccessOp::NoAccess_NoAccess, TextureViewDesc const& desc = {});
		[[maybe_unused]] RGDepthStencilId ReadDepthStencil(RGResourceName name, ERGLoadStoreAccessOp load_store_op, ERGLoadStoreAccessOp stencil_load_store_op = ERGLoadStoreAccessOp::NoAccess_NoAccess, TextureViewDesc const& desc = {});

		[[nodiscard]] RGBufferCopySrcId ReadCopySrcBuffer(RGResourceName name);
		[[nodiscard]] RGBufferCopyDstId WriteCopyDstBuffer(RGResourceName name);
		[[nodiscard]] RGBufferIndirectArgsId ReadIndirectArgsBuffer(RGResourceName name);
		[[nodiscard]] RGBufferReadOnlyId ReadBuffer(RGResourceName name, ERGReadAccess read_access, BufferViewDesc const& desc = {});
		[[nodiscard]] RGBufferReadWriteId WriteBuffer(RGResourceName name, BufferViewDesc const& desc = {});

		[[nodiscard]] RGAllocationId UseAllocation(RGResourceName name);
		
		void SetViewport(uint32 width, uint32 height);
	private:
		RenderGraphBuilder(RenderGraph&, RenderGraphPassBase&);

	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;
	};

	using RGBuilder = RenderGraphBuilder;
}