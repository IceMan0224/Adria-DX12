#pragma once
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxCommandList.h"
#include "RenderGraphResourceId.h"
#include "RenderGraphBlackboard.h"
#if RG_DEBUG
#include "Utilities/StringConversions.h"
#endif

namespace adria
{
	class RenderGraphPassBase;
	class RenderGraph;
	struct RGTextureDesc;
	struct RGBufferDesc;

	template<RGResourceType ResourceType>
	struct RGResourceTraits;

	template<>
	struct RGResourceTraits<RGResourceType::Texture>
	{
		using Resource = GfxTexture;
		using ResourceDesc = GfxTextureDesc;
	};

	template<>
	struct RGResourceTraits<RGResourceType::Buffer>
	{
		using Resource = GfxBuffer;
		using ResourceDesc = GfxBufferDesc;
	};

	struct RenderGraphResource
	{
		RenderGraphResource(Uint64 id, Bool imported, Char const* name)
			: id(id), imported(imported), version(0), ref_count(0), name(name)
		{}

		Uint64 id;
		Bool imported;
		Uint64 version;
		Uint64 ref_count;

		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
		Char const* name = "";
	};
	using RGResource = RenderGraphResource;

	template<RGResourceType ResourceType>
	struct TypedRenderGraphResource : RenderGraphResource
	{
		using Resource = RGResourceTraits<ResourceType>::Resource;
		using ResourceDesc = RGResourceTraits<ResourceType>::ResourceDesc;

		TypedRenderGraphResource(Uint64 id, Resource* resource, Char const* name = "")
			: RenderGraphResource(id, true, name), resource(resource), desc(resource->GetDesc())
		{}

		TypedRenderGraphResource(Uint64 id, ResourceDesc const& desc, Char const* name = "")
			: RenderGraphResource(id, false, name), resource(nullptr), desc(desc)
		{}

		void SetName()
		{
#if RG_DEBUG
			ADRIA_ASSERT_MSG(resource != nullptr, "Call SetDebugName at allocation/creation of a resource");
			resource->SetName(name);
#endif
		}

		Resource* resource;
		ResourceDesc desc;
	};

	using RGTexture = TypedRenderGraphResource<RGResourceType::Texture>;
	using RGBuffer = TypedRenderGraphResource<RGResourceType::Buffer>;

	class RenderGraphContext
	{
		friend RenderGraph;
	public:
		RenderGraphContext() = delete;
		ADRIA_NONCOPYABLE(RenderGraphContext)

		RGBlackboard& GetBlackboard();

		GfxDevice* GetDevice() const;
		GfxCommandList* GetCommandList() const;

		GfxTexture& GetTexture(RGTextureId res_id) const;
		GfxBuffer& GetBuffer(RGBufferId res_id) const;

		GfxTexture const& GetCopySrcTexture(RGTextureCopySrcId res_id) const;
		GfxTexture&		  GetCopyDstTexture(RGTextureCopyDstId res_id) const;
		GfxBuffer  const& GetCopySrcBuffer(RGBufferCopySrcId res_id) const;
		GfxBuffer&		  GetCopyDstBuffer(RGBufferCopyDstId res_id) const;
		GfxBuffer  const& GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const;
		GfxBuffer  const& GetVertexBuffer(RGBufferVertexId res_id) const;
		GfxBuffer  const& GetIndexBuffer(RGBufferIndexId res_id) const;
		GfxBuffer  const& GetConstantBuffer(RGBufferConstantId res_id) const;

		GfxDescriptor GetRenderTarget(RGRenderTargetId res_id) const;
		GfxDescriptor GetDepthStencil(RGDepthStencilId res_id) const;
		GfxDescriptor GetReadOnlyTexture(RGTextureReadOnlyId res_id) const;
		GfxDescriptor GetReadWriteTexture(RGTextureReadWriteId res_id) const;

		GfxDescriptor GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const;
		GfxDescriptor GetReadWriteBuffer(RGBufferReadWriteId res_id) const;
	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;
		GfxCommandList* cmd_list;

	private:
		RenderGraphContext(RenderGraph& rg, RenderGraphPassBase& rg_pass, GfxCommandList* cmd_list);
	};
	using RGContext = RenderGraphContext;
}