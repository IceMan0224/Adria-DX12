#include "D3D12Descriptor.h"
#include "D3D12DescriptorHeap.h"

namespace adria
{
	static GfxDescriptor EncodeFromD3D12Descriptor(D3D12DescriptorHeap* heap, Uint32 index)
	{
		GfxDescriptor desc{};
		desc.opaque_data[0] = reinterpret_cast<Uint64>(heap);
		desc.opaque_data[1] = static_cast<Uint64>(index);
		return desc;
	}

	GfxDescriptor EncodeFromD3D12Descriptor(D3D12Descriptor const& internal_desc)
	{
		return EncodeFromD3D12Descriptor(internal_desc.parent_heap, internal_desc.index);
	}

	D3D12Descriptor DecodeToD3D12Descriptor(GfxDescriptor const& desc)
	{
		D3D12Descriptor internal_desc{};
		internal_desc.parent_heap = reinterpret_cast<D3D12DescriptorHeap*>(desc.opaque_data[0]);
		internal_desc.index = static_cast<Uint32>(desc.opaque_data[1]);
		return internal_desc;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DecodeToD3D12CPUHandle(GfxDescriptor const& desc)
	{
		D3D12Descriptor internal_desc = DecodeToD3D12Descriptor(desc);
		return ToD3D12CPUHandle(internal_desc);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ToD3D12CPUHandle(D3D12Descriptor const& internal_desc)
	{
		if (!internal_desc.parent_heap)
		{
			return { NULL };
		}
		return internal_desc.parent_heap->GetCpuHandle(internal_desc.index);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DecodeToD3D12GPUHandle(GfxDescriptor const& desc)
	{
		D3D12Descriptor internal_desc = DecodeToD3D12Descriptor(desc);
		return ToD3D12GPUHandle(internal_desc);
	}
	D3D12_GPU_DESCRIPTOR_HANDLE ToD3D12GPUHandle(D3D12Descriptor const& internal_desc)
	{
		if (!internal_desc.parent_heap)
		{
			return { NULL };
		}
		return internal_desc.parent_heap->GetGpuHandle(internal_desc.index);
	}
}