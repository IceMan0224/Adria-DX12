#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "Graphics/GfxLinearDynamicAllocator.h"

namespace adria
{
	static VkBufferUsageFlags GetBufferUsageFlags(GfxBufferDesc const& desc)
	{
		VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
			                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			                    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::ShaderResource))
		{
			flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		}
		if (HasAnyFlag(desc.bind_flags, GfxBindFlag::UnorderedAccess))
		{
			flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
		}
		if (HasAnyFlag(desc.misc_flags, GfxBufferMiscFlag::ConstantBuffer))
		{
			flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		if (HasAnyFlag(desc.misc_flags, GfxBufferMiscFlag::VertexBuffer))
		{
			flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		}
		if (HasAnyFlag(desc.misc_flags, GfxBufferMiscFlag::IndexBuffer))
		{
			flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		}
		if (HasAnyFlag(desc.misc_flags, GfxBufferMiscFlag::IndirectArgs))
		{
			flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		}
		if (HasAnyFlag(desc.misc_flags, GfxBufferMiscFlag::AccelStruct))
		{
			flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
				   | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		}
		return flags;
	}

	VulkanBuffer::VulkanBuffer(GfxDevice* gfx, GfxBufferDesc const& desc, GfxBufferData initial_data)
		: GfxBuffer(gfx, desc)
	{
		VulkanDevice* vk_gfx = static_cast<VulkanDevice*>(gfx);
		device        = vk_gfx->GetVkDevice();
		vma_allocator = vk_gfx->GetVmaAllocator();

		VkBufferCreateInfo buffer_ci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		buffer_ci.size        = desc.size;
		buffer_ci.usage       = GetBufferUsageFlags(desc);
		buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vk_gfx->IsRayTracingSupported())
		{
			buffer_ci.usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
		}

		VmaAllocationCreateInfo alloc_ci{};
		switch (desc.resource_usage)
		{
		case GfxResourceUsage::Default:
			alloc_ci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			alloc_ci.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			break;
		case GfxResourceUsage::Upload:
			alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		case GfxResourceUsage::Readback:
			alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_ci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		}

		VmaAllocationInfo alloc_info{};
		VK_CHECK(vmaCreateBuffer(vma_allocator, &buffer_ci, &alloc_ci, &buffer, &allocation, &alloc_info));
		if (desc.resource_usage == GfxResourceUsage::Upload || desc.resource_usage == GfxResourceUsage::Readback)
		{
			mapped_data = alloc_info.pMappedData;
		}

		if (buffer_ci.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			VkBufferDeviceAddressInfo bda_info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
			bda_info.buffer = buffer;
			device_address  = vkGetBufferDeviceAddress(device, &bda_info);
		}

		if (initial_data.data && desc.resource_usage == GfxResourceUsage::Default)
		{
			GfxCommandList* cmd = gfx->GetGraphicsCommandList();
			GfxLinearDynamicAllocator* dynamic_allocator = gfx->GetDynamicAllocator();
			GfxDynamicAllocation upload_alloc = dynamic_allocator->Allocate(desc.size);
			upload_alloc.Update(initial_data.data, desc.size);
			cmd->CopyBuffer(*this, 0, *upload_alloc.buffer, upload_alloc.offset, desc.size);
		}
		else if (initial_data.data && mapped_data)
		{
			memcpy(mapped_data, initial_data.data, desc.size);
		}
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (buffer != VK_NULL_HANDLE)
		{
			vmaDestroyBuffer(vma_allocator, buffer, allocation);
		}
	}

	void* VulkanBuffer::Map()
	{
		if (mapped_data)
		{
			return mapped_data;
		}
		VK_CHECK(vmaMapMemory(vma_allocator, allocation, &mapped_data));
		return mapped_data;
	}

	void VulkanBuffer::Unmap()
	{
		if (!desc.persistent && mapped_data)
		{
			vmaUnmapMemory(vma_allocator, allocation);
			mapped_data = nullptr;
		}
	}

	void VulkanBuffer::SetName(Char const* name)
	{
		VK_OBJECT_SET_NAME(device, buffer, VK_OBJECT_TYPE_BUFFER, name);
	}
}
