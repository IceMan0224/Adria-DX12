#pragma once
#include "GfxDescriptor.h"

namespace adria
{
	struct GfxDescriptorHeapDesc
	{
		GfxDescriptorHeapType type = GfxDescriptorHeapType::Invalid;
		Uint32 descriptor_count = 0;
		Bool shader_visible = false;
	};

	class GfxDescriptorHeap
	{
	public:
		virtual ~GfxDescriptorHeap() = default;
		virtual GfxDescriptor GetDescriptor(Uint32 index = 0) const = 0;
		virtual void* GetNative() const = 0;
		virtual Uint32 GetCapacity() const = 0;
		virtual GfxDescriptorHeapType GetType() const = 0;
	};
}