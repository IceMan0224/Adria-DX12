#pragma once
#include <unordered_map>
#include <vk_mem_alloc.h>
#include "VulkanDefines.h"
#include "Graphics/GfxTexture.h"

namespace adria
{
	struct VulkanImageViewKey
	{
		Uint32 first_mip   = 0;
		Uint32 mip_count   = 1;
		Uint32 first_slice = 0;
		Uint32 slice_count = 1;
		VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
		VkImageAspectFlags aspect  = VK_IMAGE_ASPECT_COLOR_BIT;

		Bool operator==(VulkanImageViewKey const&) const = default;
	};
	struct VulkanImageViewKeyHash
	{
		Usize operator()(VulkanImageViewKey const& k) const
		{
			Usize h = std::hash<Uint32>{}(k.first_mip);
			h ^= std::hash<Uint32>{}(k.mip_count)   + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<Uint32>{}(k.first_slice)  + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<Uint32>{}(k.slice_count)  + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<Uint32>{}((Uint32)k.view_type) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<Uint32>{}(k.aspect)       + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	class VulkanTexture final : public GfxTexture
	{
	public:
		VulkanTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data);
		VulkanTexture(GfxDevice* gfx, GfxTextureDesc const& desc);
		VulkanTexture(GfxDevice* gfx, GfxTextureDesc const& desc, void* existing_image);
		virtual ~VulkanTexture() override;

		virtual void*   GetNative() const override       { return (void*)image; }
		virtual void*   GetSharedHandle() const override { return nullptr; }
		virtual Uint64  GetGpuAddress() const override   { return 0; }
		virtual void*   Map() override;
		virtual void    Unmap() override;
		virtual void    SetName(Char const* name) override;
		virtual Uint32  GetRowPitch(Uint32 mip_level = 0) const override;

		VkImage       GetImage()  const { return image; }
		VmaAllocation GetAllocation() const { return allocation; }
		VkImageView   GetView(VulkanImageViewKey const& key);
		VkImageView   GetDefaultSRVView() const { return default_srv_view; }
		VkImageView   GetDefaultUAVView() const { return default_uav_view; }
		VkImageView   GetDefaultRTVView() const { return default_rtv_view; }
		VkImageView   GetDefaultDSVView() const { return default_dsv_view; }

		Bool IsLayoutUndefined() const { return layout_undefined; }
		void MarkLayoutDefined() const { layout_undefined = false; }


	private:
		VkDevice      device        = VK_NULL_HANDLE;
		VkImage       image         = VK_NULL_HANDLE;
		VmaAllocation allocation    = VK_NULL_HANDLE;
		VmaAllocator  vma_allocator = VK_NULL_HANDLE;
		mutable Bool  layout_undefined = true;

		VkImageView   default_srv_view = VK_NULL_HANDLE;
		VkImageView   default_uav_view = VK_NULL_HANDLE;
		VkImageView   default_rtv_view = VK_NULL_HANDLE;
		VkImageView   default_dsv_view = VK_NULL_HANDLE;

		std::unordered_map<VulkanImageViewKey, VkImageView, VulkanImageViewKeyHash> view_cache;

	private:
		void CreateDefaultViews();
		VkImageView CreateImageView(VulkanImageViewKey const& key);
		void InitFromDevice(GfxDevice* gfx);
	};
}
