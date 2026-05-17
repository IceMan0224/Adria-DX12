#include "VulkanTexture.h"
#include "VulkanDevice.h"
#include "VulkanConversions.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxLinearDynamicAllocator.h"

namespace adria
{
	static VkImageType GetImageType(GfxTextureType type)
	{
		switch (type)
		{
		case GfxTextureType_1D: return VK_IMAGE_TYPE_1D;
		case GfxTextureType_2D: return VK_IMAGE_TYPE_2D;
		case GfxTextureType_3D: return VK_IMAGE_TYPE_3D;
		default:                return VK_IMAGE_TYPE_2D;
		}
	}

	static VkImageViewType GetDefaultViewType(GfxTextureDesc const& desc)
	{
		Bool const is_cube = HasAnyFlag(desc.misc_flags, GfxTextureMiscFlag::TextureCube);
		switch (desc.type)
		{
		case GfxTextureType_1D:
			return desc.array_size > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		case GfxTextureType_2D:
			if (is_cube)
			{
				return desc.array_size > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			}
			return desc.array_size > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
		case GfxTextureType_3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		default:
			return VK_IMAGE_VIEW_TYPE_2D;
		}
	}

	static VkImageUsageFlags GetImageUsageFlags(GfxTextureDesc const& desc)
	{
		VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
		{
			flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::UnorderedAccess))
		{
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;
		}
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::RenderTarget))
		{
			flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::DepthStencil))
		{
			flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		return flags;
	}

	void VulkanTexture::CreateDefaultViews()
	{
		VkFormat vk_format = ConvertFormat(desc.format);
		VkImageAspectFlags aspect = GetAspectFlags(vk_format);
		VkImageViewType view_type = GetDefaultViewType(desc);

		VulkanImageViewKey srv_key{};
		srv_key.first_mip   = 0;
		srv_key.mip_count   = desc.mip_levels;
		srv_key.first_slice = 0;
		srv_key.slice_count = desc.array_size;
		srv_key.view_type   = view_type;
		srv_key.aspect      = aspect;

		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
		{
			default_srv_view = GetView(srv_key);
		}
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::UnorderedAccess))
		{
			VulkanImageViewKey uav_key = srv_key;
			uav_key.mip_count   = 1;
			uav_key.aspect      = VK_IMAGE_ASPECT_COLOR_BIT;
			default_uav_view = GetView(uav_key);
		}
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::RenderTarget))
		{
			VulkanImageViewKey rtv_key = srv_key;
			rtv_key.mip_count   = 1;
			rtv_key.slice_count = 1;
			rtv_key.aspect      = VK_IMAGE_ASPECT_COLOR_BIT;
			default_rtv_view = GetView(rtv_key);
		}
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::DepthStencil))
		{
			VulkanImageViewKey dsv_key = srv_key;
			dsv_key.mip_count   = 1;
			dsv_key.slice_count = 1;
			dsv_key.aspect      = aspect;
			default_dsv_view = GetView(dsv_key);
		}
	}

	VkImageView VulkanTexture::CreateImageView(VulkanImageViewKey const& key)
	{
		VkImageViewCreateInfo view_ci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_ci.image                           = image;
		view_ci.viewType                        = key.view_type;
		view_ci.format                          = ConvertFormat(desc.format);
		view_ci.components                      = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
		                                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		view_ci.subresourceRange.aspectMask     = key.aspect;
		view_ci.subresourceRange.baseMipLevel   = key.first_mip;
		view_ci.subresourceRange.levelCount     = key.mip_count;
		view_ci.subresourceRange.baseArrayLayer = key.first_slice;
		view_ci.subresourceRange.layerCount     = key.slice_count;

		VkImageView view = VK_NULL_HANDLE;
		VK_CHECK(vkCreateImageView(device, &view_ci, nullptr, &view));
		return view;
	}

	VkImageView VulkanTexture::GetView(VulkanImageViewKey const& key)
	{
		auto it = view_cache.find(key);
		if (it != view_cache.end())
		{
			return it->second;
		}
		VkImageView view = CreateImageView(key);
		view_cache[key] = view;
		return view;
	}

	void VulkanTexture::InitFromDevice(GfxDevice* gfx)
	{
		VulkanDevice* vk_gfx = static_cast<VulkanDevice*>(gfx);
		device        = vk_gfx->GetVkDevice();
		vma_allocator = vk_gfx->GetVmaAllocator();

		VkImageCreateInfo image_ci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		image_ci.imageType     = GetImageType(desc.type);
		image_ci.format        = ConvertFormat(desc.format);
		image_ci.extent        = { desc.width, desc.height ? desc.height : 1u, desc.depth ? desc.depth : 1u };
		image_ci.mipLevels     = desc.mip_levels;
		image_ci.arrayLayers   = desc.array_size;
		image_ci.samples       = ConvertSampleCount(desc.sample_count);
		image_ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
		image_ci.usage         = GetImageUsageFlags(desc);
		image_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
		image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (HasAnyFlag(desc.misc_flags, GfxTextureMiscFlag::TextureCube))
		{
			image_ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		VmaAllocationCreateInfo alloc_ci{};
		alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		VK_CHECK(vmaCreateImage(vma_allocator, &image_ci, &alloc_ci, &image, &allocation, nullptr));
	}

	VulkanTexture::VulkanTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data)
		: GfxTexture(gfx, desc)
	{
		InitFromDevice(gfx);
		CreateDefaultViews();
		if (data.sub_data && data.sub_count != Uint32(-1))
		{
			constexpr Uint64 TEXEL_ALIGN = 16;
			Uint32 const depth = (desc.type == GfxTextureType_3D) ? std::max(1u, desc.depth) : 1u;
			Bool const is_3d = desc.type == GfxTextureType_3D;
			auto MipByteSize = [&](Uint32 mip, Uint64 sub_slice_pitch) -> Uint64
			{
				Uint64 slice_pitch = (sub_slice_pitch != 0)
					? sub_slice_pitch
					: GetSlicePitch(desc.format, desc.width, desc.height, mip);
				return slice_pitch * (is_3d ? std::max(1u, depth >> mip) : 1u);
			};

			Uint64 total_size = 0;
			for (Uint32 i = 0; i < data.sub_count; ++i)
			{
				Uint32 mip = i % desc.mip_levels;
				total_size = AlignUp(total_size, TEXEL_ALIGN) + MipByteSize(mip, data.sub_data[i].slice_pitch);
			}

			GfxLinearDynamicAllocator* dynamic_allocator = gfx->GetDynamicAllocator();
			GfxDynamicAllocation alloc = dynamic_allocator->Allocate(total_size, TEXEL_ALIGN);
			Uint64 offset = 0;

			GfxCommandList* cmd = gfx->GetGraphicsCommandList();
			cmd->TextureBarrier(*this, GfxResourceState::None, GfxResourceState::CopyDst);
			cmd->FlushBarriers();
			for (Uint32 i = 0; i < data.sub_count; ++i)
			{
				Uint32 mip   = i % desc.mip_levels;
				Uint32 slice = i / desc.mip_levels;
				Uint64 sp    = MipByteSize(mip, data.sub_data[i].slice_pitch);
				offset = AlignUp(offset, TEXEL_ALIGN);
				memcpy((Uint8*)alloc.cpu_address + offset, data.sub_data[i].data, sp);
				cmd->CopyBufferToTexture(*this, mip, slice, *alloc.buffer, (Uint32)(alloc.offset + offset));
				offset += sp;
			}
			if (desc.initial_state != GfxResourceState::CopyDst)
			{
				cmd->TextureBarrier(*this, GfxResourceState::CopyDst, desc.initial_state);
				cmd->FlushBarriers();
			}
		}
	}

	VulkanTexture::VulkanTexture(GfxDevice* gfx, GfxTextureDesc const& desc)
		: GfxTexture(gfx, desc)
	{
		InitFromDevice(gfx);
		CreateDefaultViews();
		if (desc.initial_state != GfxResourceState::None)
		{
			VulkanDevice* vk_gfx = static_cast<VulkanDevice*>(gfx);
			VkImageLayout target = ConvertResourceStateToLayout(desc.initial_state);
			if (target != VK_IMAGE_LAYOUT_UNDEFINED)
			{
				VkImageAspectFlags aspect = GetAspectFlags(desc.format);
				vk_gfx->TransitionImageLayoutImmediate(image, aspect, VK_IMAGE_LAYOUT_UNDEFINED, target);
				layout_undefined = false;
			}
		}
	}

	VulkanTexture::VulkanTexture(GfxDevice* gfx, GfxTextureDesc const& desc, void* existing_image)
		: GfxTexture(gfx, desc, existing_image)
	{
		VulkanDevice* vk_gfx = static_cast<VulkanDevice*>(gfx);
		device        = vk_gfx->GetVkDevice();
		vma_allocator = vk_gfx->GetVmaAllocator();
		image         = (VkImage)existing_image;
		allocation    = VK_NULL_HANDLE; 
		CreateDefaultViews();
	}

	VulkanTexture::~VulkanTexture()
	{
		for (auto& [key, view] : view_cache)
		{
			vkDestroyImageView(device, view, nullptr);
		}
		view_cache.clear();

		if (!is_backbuffer && image != VK_NULL_HANDLE)
		{
			vmaDestroyImage(vma_allocator, image, allocation);
		}
	}

	void* VulkanTexture::Map()
	{
		if (mapped_data)
		{
			return mapped_data;
		}
		VK_CHECK(vmaMapMemory(vma_allocator, allocation, &mapped_data));
		return mapped_data;
	}

	void VulkanTexture::Unmap()
	{
		if (!desc.persistent && mapped_data)
		{
			vmaUnmapMemory(vma_allocator, allocation);
			mapped_data = nullptr;
		}
	}

	void VulkanTexture::SetName(Char const* name)
	{
		VK_OBJECT_SET_NAME(device, image, VK_OBJECT_TYPE_IMAGE, name);
	}

	Uint32 VulkanTexture::GetRowPitch(Uint32 mip_level) const
	{
		Uint32 width = std::max(1u, desc.width >> mip_level);
		VkFormat vk_format = ConvertFormat(desc.format);
		VkImageSubresource sub{ VK_IMAGE_ASPECT_COLOR_BIT, mip_level, 0 };
		VkSubresourceLayout layout{};
		vkGetImageSubresourceLayout(device, image, &sub, &layout);
		return (Uint32)layout.rowPitch;
	}
}
