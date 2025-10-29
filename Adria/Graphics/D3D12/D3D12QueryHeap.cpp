#include "D3D12QueryHeap.h"
#include "D3D12Defines.h"
#include "D3D12Device.h"

namespace adria
{
	inline constexpr D3D12_QUERY_HEAP_TYPE ToD3D12QueryHeapType(GfxQueryType query_type)
	{
		switch (query_type)
		{
		case GfxQueryType::Timestamp:
			return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		case GfxQueryType::Occlusion:
		case GfxQueryType::BinaryOcclusion:
			return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
		case GfxQueryType::PipelineStatistics:
			return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
		}
		return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	}


	D3D12QueryHeap::D3D12QueryHeap(GfxDevice* gfx, GfxQueryHeapDesc const& desc) : GfxQueryHeap(gfx, desc)
	{
		D3D12_QUERY_HEAP_DESC heap_desc{};
		heap_desc.Count = desc.count;
		heap_desc.NodeMask = 0;
		heap_desc.Type = ToD3D12QueryHeapType(desc.type);
		D3D12_CHECK_CALL(((D3D12Device*)gfx)->GetD3D12Device()->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(query_heap.GetAddressOf())));
	}
}
