#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxBuffer.h"
#include <vk_mem_alloc.h>

namespace adria
{
	class VulkanBuffer final : public GfxBuffer
	{
	public:
		VulkanBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data = {});
		virtual ~VulkanBuffer() override;

		virtual void*   GetNative() const override       { return (void*)buffer; }
		virtual Uint64  GetGpuAddress() const override   { return device_address; }
		virtual void*   GetSharedHandle() const override { return nullptr; }
		virtual void*   Map() override;
		virtual void    Unmap() override;
		virtual void    SetName(Char const* name) override;

		VkBuffer      GetBuffer()     const { return buffer; }
		VmaAllocation GetAllocation() const { return allocation; }

	private:
		VkDevice      device         = VK_NULL_HANDLE;
		VkBuffer      buffer         = VK_NULL_HANDLE;
		VmaAllocation allocation     = VK_NULL_HANDLE;
		VkDeviceAddress device_address = 0;
		VmaAllocator  vma_allocator  = VK_NULL_HANDLE;
	};
}
