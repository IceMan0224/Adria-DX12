#pragma once
#include "D3D12Descriptor.h"

namespace adria
{
	class D3D12Device;

	struct D3D12DescriptorHeapDesc
	{
		GfxDescriptorType type = GfxDescriptorType::Invalid;
		Uint32 descriptor_count = 0;
		Bool shader_visible = false;
	};

	class D3D12DescriptorHeap final 
	{
	public:
		D3D12DescriptorHeap(D3D12Device* d3d12_gfx, D3D12DescriptorHeapDesc const& desc);
		~D3D12DescriptorHeap() = default;

		ADRIA_FORCEINLINE Uint32			GetCapacity() const { return descriptor_count; }
		ADRIA_FORCEINLINE GfxDescriptorType GetType() const { return type; }
		ADRIA_FORCEINLINE Bool				IsShaderVisible() const { return shader_visible; }
		ADRIA_FORCEINLINE ID3D12DescriptorHeap* GetD3D12Heap() const { return heap.Get(); }

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(Uint32 index) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(Uint32 index) const;
		D3D12Descriptor GetDescriptor(Uint32 index) const;

	private:
		D3D12Device* d3d12_gfx;
		Ref<ID3D12DescriptorHeap> heap;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_start_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_start_handle;
		Uint32 descriptor_handle_size;
		Uint32 descriptor_count;
		GfxDescriptorType type;
		Bool shader_visible;

	private:
		void CreateHeap();
	};
}