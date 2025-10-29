#include "D3D12DescriptorHeap.h"
#include "D3D12Defines.h"
#include "D3D12Device.h"

namespace adria
{
	inline constexpr D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12HeapType(GfxDescriptorType type)
	{
		switch (type)
		{
		case GfxDescriptorType::CBV_SRV_UAV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		case GfxDescriptorType::Sampler:
			return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		case GfxDescriptorType::RTV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case GfxDescriptorType::DSV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		}
		ADRIA_ASSERT_MSG(false, "Invalid Descriptor Heap Type!");
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	}

	D3D12DescriptorHeap::D3D12DescriptorHeap(D3D12Device* gfx, D3D12DescriptorHeapDesc const& desc)
		: d3d12_gfx(gfx), descriptor_count(desc.descriptor_count), type(desc.type), shader_visible(desc.shader_visible)
	{
		cpu_start_handle = { 0 };
		gpu_start_handle = { 0 };
		CreateHeap();
	}

	void D3D12DescriptorHeap::CreateHeap()
	{
		ADRIA_ASSERT(descriptor_count <= UINT32_MAX && "Too many descriptors");
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heap_desc.NumDescriptors = descriptor_count;
		heap_desc.Type = ToD3D12HeapType(type);

		ID3D12Device* device = d3d12_gfx->GetD3D12Device();
		D3D12_CHECK_CALL(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())));

		cpu_start_handle = heap->GetCPUDescriptorHandleForHeapStart();
		if (shader_visible)
		{
			gpu_start_handle = heap->GetGPUDescriptorHandleForHeapStart();
		}
		descriptor_handle_size = device->GetDescriptorHandleIncrementSize(heap_desc.Type);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetCpuHandle(Uint32 index) const
	{
		ADRIA_ASSERT(index < descriptor_count);
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = cpu_start_handle;
		cpu_handle.ptr += static_cast<SIZE_T>(index) * descriptor_handle_size;
		return cpu_handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetGpuHandle(Uint32 index) const
	{
		ADRIA_ASSERT(index < descriptor_count && "Index out of bounds");
		ADRIA_ASSERT(shader_visible && "Attempting to get GPU handle from a non-shader-visible heap");

		if (!shader_visible)
		{
			return { 0 }; 
		}
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = gpu_start_handle;
		gpu_handle.ptr += static_cast<SIZE_T>(index) * descriptor_handle_size;
		return gpu_handle;
	}

	D3D12Descriptor D3D12DescriptorHeap::GetDescriptor(Uint32 index) const
	{
		D3D12Descriptor desc{};
		desc.parent_heap = const_cast<D3D12DescriptorHeap*>(this);
		desc.index = index;
		return desc;
	}

}

