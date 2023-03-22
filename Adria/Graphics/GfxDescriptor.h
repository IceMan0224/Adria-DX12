#pragma once
#include "../Core/Definitions.h"
#include <d3d12.h>

namespace adria
{
	class GfxDescriptor
	{
		friend class GfxDescriptorAllocatorBase;

	public:
		GfxDescriptor() {}
		GfxDescriptor(GfxDescriptor const&) = default;
		GfxDescriptor(GfxDescriptor&&) = default;
		GfxDescriptor& operator=(GfxDescriptor const&) = default;
		GfxDescriptor& operator=(GfxDescriptor&&) = default;

		operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return cpu; }
		operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return gpu; }


		uint32 GetIndex() const { return index; }
		void Increment(uint32 increment, uint32 multiply = 1)
		{
			cpu.ptr += increment * multiply;
			if(gpu.ptr != NULL) gpu.ptr += increment * multiply;
			index += multiply;
		}

		bool operator==(GfxDescriptor const& other)
		{
			return cpu.ptr == other.cpu.ptr && index == other.index;
		}

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = { NULL };
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = { NULL };
		uint32 index = -1;
	};
}