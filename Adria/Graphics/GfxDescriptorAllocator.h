#pragma once
#include "GfxDescriptorAllocatorBase.h"

namespace adria
{
	struct GfxDescriptorAllocatorDesc
	{
		GfxDescriptorHeapType type = GfxDescriptorHeapType::Invalid;
		Uint32 descriptor_count = 0;
		Bool shader_visible = false;
	};

	class GfxDescriptorAllocator : public GfxDescriptorAllocatorBase
	{
		struct GfxDescriptorRange
		{
			GfxDescriptor begin;
			GfxDescriptor end;
		};

	public:
		GfxDescriptorAllocator(GfxDevice* gfx_device, GfxDescriptorAllocatorDesc const& desc);
		~GfxDescriptorAllocator();

		ADRIA_NODISCARD GfxDescriptor AllocateDescriptor();
		void FreeDescriptor(GfxDescriptor handle);

	private:
		GfxDescriptor tail_descriptor;
		std::list<GfxDescriptorRange> free_descriptor_ranges;
	};
}