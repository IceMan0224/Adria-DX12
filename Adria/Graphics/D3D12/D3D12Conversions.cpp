#include "D3D12Conversions.h"
#include "D3D12DescriptorHeap.h"

namespace adria
{
	D3D12_CPU_DESCRIPTOR_HANDLE ToD3D12CpuHandle(GfxDescriptor descriptor)
	{
		if (!descriptor.IsValid())
		{
			return D3D12_CPU_DESCRIPTOR_HANDLE{};
		}
		D3D12DescriptorHeap* d3d12_heap = static_cast<D3D12DescriptorHeap*>(descriptor.parent_heap);
		return d3d12_heap->GetCpuHandle(descriptor);
	}
	D3D12_GPU_DESCRIPTOR_HANDLE ToD3D12GpuHandle(GfxDescriptor descriptor)
	{
		if (!descriptor.IsValid())
		{
			return D3D12_GPU_DESCRIPTOR_HANDLE{};
		}
		D3D12DescriptorHeap* d3d12_heap = static_cast<D3D12DescriptorHeap*>(descriptor.parent_heap);
		return d3d12_heap->GetGpuHandle(descriptor);
	}
}