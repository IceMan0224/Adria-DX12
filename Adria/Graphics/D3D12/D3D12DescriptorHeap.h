#pragma once
#include "Graphics/GfxDescriptorHeap.h"

namespace adria
{
	class GfxDevice; 
	class D3D12DescriptorHeap : public GfxDescriptorHeap
	{
	public:
		D3D12DescriptorHeap(GfxDevice* gfx, GfxDescriptorHeapDesc const& desc);
		virtual ~D3D12DescriptorHeap() = default;

		virtual GfxDescriptor GetDescriptor(Uint32 index = 0) const override;
		ADRIA_FORCEINLINE virtual void*		GetHandle() const override { return heap.Get(); }
		ADRIA_FORCEINLINE virtual Uint32	GetCapacity() const override { return descriptor_count; }
		ADRIA_FORCEINLINE virtual GfxDescriptorHeapType GetType() const override { return type; }

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(GfxDescriptor const& handle) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(GfxDescriptor const& handle) const;
		ID3D12DescriptorHeap*		GetD3D12Heap() const { return heap.Get(); }

	private:
		GfxDevice* gfx;
		Ref<ID3D12DescriptorHeap> heap;
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_start_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_start_handle;
		Uint32 descriptor_handle_size;
		Uint32 descriptor_count;
		GfxDescriptorHeapType type;
		Bool shader_visible;

	private:
		void CreateHeap();
	};
}