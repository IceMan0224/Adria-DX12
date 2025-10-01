#include "D3D12DescriptorHeap.h"

namespace adria
{
	inline constexpr D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12HeapType(GfxDescriptorHeapType type)
	{
		switch (type)
		{
		case GfxDescriptorHeapType::CBV_SRV_UAV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		case GfxDescriptorHeapType::Sampler:
			return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		case GfxDescriptorHeapType::RTV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case GfxDescriptorHeapType::DSV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		}
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	}

	D3D12DescriptorHeap::D3D12DescriptorHeap(GfxDevice* gfx, GfxDescriptorHeapDesc const& desc)
		: gfx(gfx), descriptor_count(desc.descriptor_count), type(desc.type), shader_visible(desc.shader_visible)
	{
		cpu_start_handle = { 0 };
		gpu_start_handle = { 0 };
		CreateHeap();
	}

	void D3D12DescriptorHeap::CreateHeap()
	{
		// ADRIA_ASSERT(descriptor_count <= UINT32_MAX && "Too many descriptors");
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heap_desc.NumDescriptors = descriptor_count;
		heap_desc.Type = ToD3D12HeapType(type);

		ID3D12Device* device = gfx->GetDevice(); // Assuming GfxDevice has this method
		// GFX_CHECK_HR(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())));

		cpu_start_handle = heap->GetCPUDescriptorHandleForHeapStart();
		if (shader_visible)
		{
			gpu_start_handle = heap->GetGPUDescriptorHandleForHeapStart();
		}
		descriptor_handle_size = device->GetDescriptorHandleIncrementSize(heap_desc.Type);
	}

	GfxDescriptor D3D12DescriptorHeap::GetHandle(uint32_t index) const
	{
		// ADRIA_ASSERT(index < descriptor_count);
		// Note: The const_cast is safe here because the GfxDescriptor doesn't modify the heap.
		return GfxDescriptor{ const_cast<D3D12DescriptorHeap*>(this), index };
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetCpuHandle(GfxDescriptor const& handle) const
	{
		// ADRIA_ASSERT(handle.IsValid() && handle.parent_heap == this);
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = cpu_start_handle;
		cpu_handle.ptr += static_cast<SIZE_T>(handle.index) * descriptor_handle_size;
		return cpu_handle;
	}



	D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetGpuHandle(GfxDescriptor const& handle) const
	{
		// ADRIA_ASSERT(handle.IsValid() && handle.parent_heap == this && shader_visible);
		if (!shader_visible) return { 0 };
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = gpu_start_handle;
		gpu_handle.ptr += static_cast<SIZE_T>(handle.index) * descriptor_handle_size;
		return gpu_handle;
	}

}

