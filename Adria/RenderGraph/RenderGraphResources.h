#pragma once
#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"
#include "RenderGraphResourceId.h"
#include "RenderGraphBlackboard.h"

namespace adria
{
	class RenderGraphPassBase;
	class RenderGraph;

	template<ERGResourceType ResourceType>
	struct RGResourceTraits;

	template<>
	struct RGResourceTraits<ERGResourceType::Texture>
	{
		using Resource = Texture;
		using ResourceDesc = TextureDesc;
	};

	template<>
	struct RGResourceTraits<ERGResourceType::Buffer>
	{
		using Resource = Buffer;
		using ResourceDesc = BufferDesc;
	};

	struct RenderGraphResource
	{
		RenderGraphResource(size_t id, bool imported)
			: id(id), imported(imported), version(0), ref_count(0)
		{}

		size_t id;
		bool imported;
		size_t version;
		size_t ref_count;

		RenderGraphPassBase* writer = nullptr;
		RenderGraphPassBase* last_used_by = nullptr;
	};

	template<ERGResourceType ResourceType>
	struct TypedRenderGraphResource : RenderGraphResource
	{
		using Resource = RGResourceTraits<ResourceType>::Resource;
		using ResourceDesc = RGResourceTraits<ResourceType>::ResourceDesc;

		TypedRenderGraphResource(size_t id, Resource* resource)
			: RenderGraphResource(id, true), resource(resource), desc(resource->GetDesc())
		{}

		TypedRenderGraphResource(size_t id, ResourceDesc const& desc)
			: RenderGraphResource(id, false), resource(nullptr), desc(desc)
		{}

		Resource* resource;
		ResourceDesc desc;
	};
	using RGTexture = TypedRenderGraphResource<ERGResourceType::Texture>;
	using RGBuffer = TypedRenderGraphResource<ERGResourceType::Buffer>;
	using RGDescriptor = D3D12_CPU_DESCRIPTOR_HANDLE;
	using RGResourceState = D3D12_RESOURCE_STATES;
	using RGCommandList = ID3D12GraphicsCommandList4;

	class RenderGraphResources
	{
		friend RenderGraph;
	public:
		RenderGraphResources() = delete;
		RenderGraphResources(RenderGraphResources const&) = delete;
		RenderGraphResources& operator=(RenderGraphResources const&) = delete;

		RGBlackboard& GetBlackboard();

		Texture const& GetTexture(RGTextureId res_id) const;
		Buffer  const& GetBuffer(RGBufferId res_id) const;

		Texture const& GetCopySrcTexture(RGTextureCopySrcId res_id) const;
		Texture const& GetCopyDstTexture(RGTextureCopyDstId res_id) const;
		Buffer  const& GetCopySrcBuffer(RGBufferCopySrcId res_id) const;
		Buffer  const& GetCopyDstBuffer(RGBufferCopyDstId res_id) const;
		Buffer  const& GetIndirectArgsBuffer(RGBufferIndirectArgsId res_id) const;

		RGDescriptor GetRenderTarget(RGRenderTargetId res_id) const;
		RGDescriptor GetDepthStencil(RGDepthStencilId res_id) const;
		RGDescriptor GetReadOnlyTexture(RGTextureReadOnlyId res_id) const;
		RGDescriptor GetReadWriteTexture(RGTextureReadWriteId res_id) const;

		RGDescriptor GetReadOnlyBuffer(RGBufferReadOnlyId res_id) const;
		RGDescriptor GetReadWriteBuffer(RGBufferReadWriteId res_id) const;

		DynamicAllocation& GetAllocation(RGAllocationId);
	private:
		RenderGraph& rg;
		RenderGraphPassBase& rg_pass;

	private:
		RenderGraphResources(RenderGraph& rg, RenderGraphPassBase& rg_pass);
	};
}