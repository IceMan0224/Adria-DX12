#pragma once
#include "Graphics/GfxDescriptor.h"

namespace adria
{
	class D3D12DescriptorHeap;
	struct D3D12Descriptor
	{
		D3D12DescriptorHeap* parent_heap = nullptr;
		Uint32 index = static_cast<Uint32>(-1);

		void Increment(Uint32 multiply = 1) { index += multiply; }
		Bool operator==(D3D12Descriptor const& other) const
		{
			return parent_heap == other.parent_heap && index == other.index;
		}
		Bool IsValid() const
		{
			return parent_heap != nullptr;
		}
	};

	GfxDescriptor   EncodeFromD3D12Descriptor(D3D12Descriptor const& internal_desc);
	D3D12Descriptor DecodeToD3D12Descriptor(GfxDescriptor const& desc);
	D3D12_CPU_DESCRIPTOR_HANDLE DecodeToD3D12CPUHandle(GfxDescriptor const& desc);
	D3D12_CPU_DESCRIPTOR_HANDLE ToD3D12CPUHandle(D3D12Descriptor const& internal_desc);
	D3D12_GPU_DESCRIPTOR_HANDLE DecodeToD3D12GPUHandle(GfxDescriptor const& desc);
	D3D12_GPU_DESCRIPTOR_HANDLE ToD3D12GPUHandle(D3D12Descriptor const& internal_desc);
}