#include "VulkanRayTracingAS.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanConversions.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"

namespace adria
{
	namespace
	{
		VkBuildAccelerationStructureFlagsKHR ConvertASFlags(GfxRayTracingASFlags flags)
		{
			VkBuildAccelerationStructureFlagsKHR vk_flags = 0;
			if (flags & GfxRayTracingASFlag_AllowUpdate)     vk_flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
			if (flags & GfxRayTracingASFlag_AllowCompaction) vk_flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
			if (flags & GfxRayTracingASFlag_PreferFastTrace) vk_flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			if (flags & GfxRayTracingASFlag_PreferFastBuild) vk_flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
			if (flags & GfxRayTracingASFlag_MinimizeMemory)  vk_flags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;
			return vk_flags;
		}

		Uint32 ConvertInstanceFlags(GfxRayTracingInstanceFlags flags)
		{
			Uint32 vk_flags = 0;
			if (flags & GfxRayTracingInstanceFlag_CullDisable)   vk_flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			if (flags & GfxRayTracingInstanceFlag_FrontCCW)      vk_flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;
			if (flags & GfxRayTracingInstanceFlag_ForceOpaque)   vk_flags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
			if (flags & GfxRayTracingInstanceFlag_ForceNoOpaque) vk_flags |= VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;
			return vk_flags;
		}

		VkIndexType ConvertIndexFormat(GfxFormat format)
		{
			switch (format)
			{
			case GfxFormat::R16_UINT: return VK_INDEX_TYPE_UINT16;
			case GfxFormat::R32_UINT: return VK_INDEX_TYPE_UINT32;
			case GfxFormat::UNKNOWN:  return VK_INDEX_TYPE_NONE_KHR;
			default:
				ADRIA_ASSERT(false && "Unsupported RT index format");
				return VK_INDEX_TYPE_NONE_KHR;
			}
		}

		std::unique_ptr<GfxBuffer> CreateASResultBuffer(GfxDevice* gfx, Uint64 size)
		{
			GfxBufferDesc desc{};
			desc.size       = size;
			desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource;
			desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
			desc.stride     = 4;
			return gfx->CreateBuffer(desc);
		}

		std::unique_ptr<GfxBuffer> CreateASScratchBuffer(GfxDevice* gfx, Uint64 size, Uint32 alignment)
		{
			GfxBufferDesc desc{};
			//Scratch must be aligned to minAccelerationStructureScratchOffsetAlignment. VMA's default
			//storage-buffer alignment is usually sufficient but we over-size by `alignment` so the
			//caller can round the BDA up safely if it isn't
			desc.size       = size + alignment;
			desc.bind_flags = GfxBindFlag::UnorderedAccess;
			desc.stride     = 4;
			return gfx->CreateBuffer(desc);
		}

		VkDeviceAddress AlignAddress(VkDeviceAddress address, Uint32 alignment)
		{
			Uint64 mask = Uint64(alignment) - 1;
			return (address + mask) & ~mask;
		}
	}

	VulkanRayTracingBLAS::VulkanRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags)
	{
		vk_device = static_cast<VulkanDevice*>(gfx);
		ADRIA_ASSERT(vk_device->IsRayTracingSupported());

		build_flags = ConvertASFlags(flags);
		geometries_vk.reserve(geometries.size());
		build_ranges.reserve(geometries.size());
		primitive_counts.reserve(geometries.size());
		for (GfxRayTracingGeometry const& g : geometries)
		{
			ADRIA_ASSERT(g.vertex_buffer != nullptr);
			ADRIA_ASSERT(g.index_format == GfxFormat::UNKNOWN || g.index_buffer != nullptr);

			VkAccelerationStructureGeometryKHR geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
			geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
			geom.flags        = g.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

			VkAccelerationStructureGeometryTrianglesDataKHR& tri = geom.geometry.triangles;
			tri = VkAccelerationStructureGeometryTrianglesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
			tri.vertexFormat            = ConvertFormat(g.vertex_format);
			tri.vertexStride            = g.vertex_stride;
			tri.maxVertex               = g.vertex_count > 0 ? g.vertex_count - 1 : 0;
			tri.vertexData.deviceAddress = g.vertex_buffer->GetGpuAddress() + g.vertex_buffer_offset;
			tri.indexType               = ConvertIndexFormat(g.index_format);
			tri.indexData.deviceAddress = g.index_buffer ? (g.index_buffer->GetGpuAddress() + g.index_buffer_offset) : 0;
			tri.transformData.deviceAddress = 0;

			geometries_vk.push_back(geom);
			Uint32 prim_count = (g.index_format == GfxFormat::UNKNOWN)
				? (g.vertex_count / 3)
				: (g.index_count / 3);
			primitive_counts.push_back(prim_count);

			VkAccelerationStructureBuildRangeInfoKHR range{};
			range.primitiveCount  = prim_count;
			range.primitiveOffset = 0;
			range.firstVertex     = 0;
			range.transformOffset = 0;
			build_ranges.push_back(range);
		}

		VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		build_info.flags         = build_flags;
		build_info.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_info.geometryCount = (Uint32)geometries_vk.size();
		build_info.pGeometries   = geometries_vk.data();

		VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		pfn_vkGetAccelerationStructureBuildSizesKHR(
			vk_device->GetVkDevice(),
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&build_info,
			primitive_counts.data(),
			&sizes);
		ADRIA_ASSERT(sizes.accelerationStructureSize > 0);

		Uint32 scratch_alignment = vk_device->GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
		if (scratch_alignment == 0) 
		{ 
			scratch_alignment = 256; 
		}

		result_buffer  = CreateASResultBuffer(gfx, sizes.accelerationStructureSize);
		scratch_buffer = CreateASScratchBuffer(gfx, sizes.buildScratchSize, scratch_alignment);
		result_buffer->SetName("BLAS result buffer");
		scratch_buffer->SetName("BLAS scratch buffer");

		VulkanBuffer* result_vk = static_cast<VulkanBuffer*>(result_buffer.get());
		VkAccelerationStructureCreateInfoKHR as_ci{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		as_ci.buffer = result_vk->GetBuffer();
		as_ci.offset = 0;
		as_ci.size   = sizes.accelerationStructureSize;
		as_ci.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		VK_CHECK(pfn_vkCreateAccelerationStructureKHR(vk_device->GetVkDevice(), &as_ci, nullptr, &as));

		VkAccelerationStructureDeviceAddressInfoKHR addr_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addr_info.accelerationStructure = as;
		as_device_address = pfn_vkGetAccelerationStructureDeviceAddressKHR(vk_device->GetVkDevice(), &addr_info);
	}

	VulkanRayTracingBLAS::~VulkanRayTracingBLAS()
	{
		if (as != VK_NULL_HANDLE)
		{
			pfn_vkDestroyAccelerationStructureKHR(vk_device->GetVkDevice(), as, nullptr);
			as = VK_NULL_HANDLE;
		}
	}


	VulkanRayTracingTLAS::VulkanRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags)
	{
		vk_device = static_cast<VulkanDevice*>(gfx);
		ADRIA_ASSERT(vk_device->IsRayTracingSupported());

		build_flags    = ConvertASFlags(flags);
		instance_count = (Uint32)instances.size();

		GfxBufferDesc instance_desc{};
		instance_desc.size           = sizeof(VkAccelerationStructureInstanceKHR) * std::max<Uint32>(instance_count, 1u);
		instance_desc.resource_usage = GfxResourceUsage::Upload;
		instance_desc.misc_flags     = GfxBufferMiscFlag::AccelStruct;
		instance_desc.stride         = sizeof(VkAccelerationStructureInstanceKHR);
		instance_buffer = gfx->CreateBuffer(instance_desc);
		instance_buffer->SetName("TLAS instance buffer");

		VkAccelerationStructureInstanceKHR* mapped = instance_buffer->GetMappedData<VkAccelerationStructureInstanceKHR>();
		ADRIA_ASSERT(mapped != nullptr);
		for (Uint32 i = 0; i < instance_count; ++i)
		{
			GfxRayTracingInstance const& src = instances[i];
			VkAccelerationStructureInstanceKHR& dst = mapped[i];
			std::memcpy(&dst.transform, &src.transform, sizeof(dst.transform));
			dst.instanceCustomIndex                    = src.instance_id & 0x00FFFFFFu;
			dst.mask                                   = src.instance_mask;
			dst.instanceShaderBindingTableRecordOffset = 0;
			dst.flags                                  = ConvertInstanceFlags(src.flags);
			dst.accelerationStructureReference         = src.blas ? src.blas->GetGpuAddress() : 0;
		}

		VkAccelerationStructureGeometryKHR tlas_geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		tlas_geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlas_geom.flags        = 0;
		tlas_geom.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		tlas_geom.geometry.instances.arrayOfPointers    = VK_FALSE;
		tlas_geom.geometry.instances.data.deviceAddress = instance_buffer->GetGpuAddress();

		VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		build_info.flags         = build_flags;
		build_info.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_info.geometryCount = 1;
		build_info.pGeometries   = &tlas_geom;

		VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		pfn_vkGetAccelerationStructureBuildSizesKHR(
			vk_device->GetVkDevice(),
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&build_info,
			&instance_count,
			&sizes);
		ADRIA_ASSERT(sizes.accelerationStructureSize > 0);

		Uint32 scratch_alignment = vk_device->GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
		if (scratch_alignment == 0) 
		{ 
			scratch_alignment = 256; 
		}

		result_buffer  = CreateASResultBuffer(gfx, sizes.accelerationStructureSize);
		Uint64 scratch_size = std::max<Uint64>(sizes.buildScratchSize, sizes.updateScratchSize);
		scratch_buffer = CreateASScratchBuffer(gfx, scratch_size, scratch_alignment);
		result_buffer->SetName("TLAS result buffer");
		scratch_buffer->SetName("TLAS scratch buffer");

		VulkanBuffer* result_vk = static_cast<VulkanBuffer*>(result_buffer.get());
		VkAccelerationStructureCreateInfoKHR as_ci{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		as_ci.buffer = result_vk->GetBuffer();
		as_ci.offset = 0;
		as_ci.size   = sizes.accelerationStructureSize;
		as_ci.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		VK_CHECK(pfn_vkCreateAccelerationStructureKHR(vk_device->GetVkDevice(), &as_ci, nullptr, &as));

		VkAccelerationStructureDeviceAddressInfoKHR addr_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addr_info.accelerationStructure = as;
		as_device_address = pfn_vkGetAccelerationStructureDeviceAddressKHR(vk_device->GetVkDevice(), &addr_info);
	}

	VulkanRayTracingTLAS::~VulkanRayTracingTLAS()
	{
		if (as != VK_NULL_HANDLE)
		{
			pfn_vkDestroyAccelerationStructureKHR(vk_device->GetVkDevice(), as, nullptr);
			as = VK_NULL_HANDLE;
		}
	}

	void VulkanRayTracingTLAS::UpdateInstances(std::span<GfxRayTracingInstance> instances)
	{
		ADRIA_ASSERT(instances.size() <= instance_count && "TLAS update with more instances than allocated");
		VkAccelerationStructureInstanceKHR* mapped = instance_buffer->GetMappedData<VkAccelerationStructureInstanceKHR>();
		ADRIA_ASSERT(mapped != nullptr);
		for (Uint64 i = 0; i < instances.size(); ++i)
		{
			std::memcpy(&mapped[i].transform, &instances[i].transform, sizeof(mapped[i].transform));
		}
	}
}
