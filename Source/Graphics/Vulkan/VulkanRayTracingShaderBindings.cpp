#include "VulkanRayTracingShaderBindings.h"
#include "VulkanRayTracingPipeline.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxBuffer.h"
#include "Utilities/Align.h"

namespace adria
{
	VulkanRayTracingShaderBindings::VulkanRayTracingShaderBindings(VulkanRayTracingPipeline const* in_pipeline)
		: pipeline(in_pipeline)
	{
		ADRIA_ASSERT(pipeline != nullptr);
		ADRIA_ASSERT(pipeline->IsValid());
	}

	void VulkanRayTracingShaderBindings::SetRayGenShader(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(!committed);
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(pipeline->GetShaderGroupHandle(name) != nullptr && "Ray generation shader not found in pipeline");
		ray_gen.name = name;
		ray_gen.local_data.assign((Uint8 const*)local_data, (Uint8 const*)local_data + data_size);
	}

	GfxShaderGroupHandle VulkanRayTracingShaderBindings::AddMissShader(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(!committed);
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(pipeline->GetShaderGroupHandle(name) != nullptr && "Miss shader not found in pipeline");
		Uint32 index = (Uint32)miss.size();
		Record r;
		r.name = name;
		r.local_data.assign((Uint8 const*)local_data, (Uint8 const*)local_data + data_size);
		miss.push_back(std::move(r));

		Uint32 record_bytes = pipeline->GetShaderGroupHandleSize() + data_size;
		miss_record_size = std::max(miss_record_size, record_bytes);
		return GfxShaderGroupHandle(index);
	}

	GfxShaderGroupHandle VulkanRayTracingShaderBindings::AddHitGroup(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(!committed);
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(pipeline->GetShaderGroupHandle(name) != nullptr && "Hit group not found in pipeline");
		Uint32 index = (Uint32)hit.size();
		Record r;
		r.name = name;
		r.local_data.assign((Uint8 const*)local_data, (Uint8 const*)local_data + data_size);
		hit.push_back(std::move(r));

		Uint32 record_bytes = pipeline->GetShaderGroupHandleSize() + data_size;
		hit_record_size = std::max(hit_record_size, record_bytes);
		return GfxShaderGroupHandle(index);
	}

	GfxShaderGroupHandle VulkanRayTracingShaderBindings::AddCallableShader(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(!committed);
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(pipeline->GetShaderGroupHandle(name) != nullptr && "Callable shader not found in pipeline");
		Uint32 index = (Uint32)callable.size();
		Record r;
		r.name = name;
		r.local_data.assign((Uint8 const*)local_data, (Uint8 const*)local_data + data_size);
		callable.push_back(std::move(r));

		Uint32 record_bytes = pipeline->GetShaderGroupHandleSize() + data_size;
		callable_record_size = std::max(callable_record_size, record_bytes);
		return GfxShaderGroupHandle(index);
	}

	void VulkanRayTracingShaderBindings::Commit()
	{
		ADRIA_ASSERT(!committed);
		ADRIA_ASSERT(!ray_gen.name.empty() && "Ray generation shader must be set before Commit()");
		committed = true;
	}

	void VulkanRayTracingShaderBindings::Build(
		GfxLinearDynamicAllocator& allocator,
		VkStridedDeviceAddressRegionKHR& raygen_region,
		VkStridedDeviceAddressRegionKHR& miss_region,
		VkStridedDeviceAddressRegionKHR& hit_region,
		VkStridedDeviceAddressRegionKHR& callable_region)
	{
		ADRIA_ASSERT(committed);

		Uint32 const handle_size      = pipeline->GetShaderGroupHandleSize();
		Uint32 const handle_alignment = pipeline->GetShaderGroupHandleAlignment();
		Uint32 const base_alignment   = pipeline->GetShaderGroupBaseAlignment();

		// Each record's stride is the handle + per-record data, rounded up to handleAlignment.
		// The ray-gen region has a single record whose size *must equal* its stride per spec, and
		// the region base must be aligned to baseAlignment.
		Uint32 const raygen_record_stride   = (Uint32)AlignUpPow2(handle_size + (Uint32)ray_gen.local_data.size(), handle_alignment);
		Uint32 const miss_stride            = miss_record_size     ? (Uint32)AlignUpPow2(miss_record_size,     handle_alignment) : 0;
		Uint32 const hit_stride             = hit_record_size      ? (Uint32)AlignUpPow2(hit_record_size,      handle_alignment) : 0;
		Uint32 const callable_stride        = callable_record_size ? (Uint32)AlignUpPow2(callable_record_size, handle_alignment) : 0;
		Uint32 const raygen_region_size     = (Uint32)AlignUpPow2(raygen_record_stride, base_alignment);

		Uint64 const miss_region_size     = (Uint64)AlignUpPow2((Uint64)miss_stride     * miss.size(),     base_alignment);
		Uint64 const hit_region_size      = (Uint64)AlignUpPow2((Uint64)hit_stride      * hit.size(),      base_alignment);
		Uint64 const callable_region_size = (Uint64)AlignUpPow2((Uint64)callable_stride * callable.size(), base_alignment);

		Uint64 const total_size = raygen_region_size + miss_region_size + hit_region_size + callable_region_size;
		GfxDynamicAllocation alloc = allocator.Allocate(total_size, base_alignment);
		ADRIA_ASSERT(alloc.cpu_address != nullptr && alloc.gpu_address != 0);
		std::memset(alloc.cpu_address, 0, total_size);

		Uint8* cpu  = (Uint8*)alloc.cpu_address;
		Uint64 addr = alloc.gpu_address;

		// Raygen (single record).
		{
			void const* handle = pipeline->GetShaderGroupHandle(ray_gen.name.c_str());
			ADRIA_ASSERT(handle != nullptr);
			std::memcpy(cpu, handle, handle_size);
			if (!ray_gen.local_data.empty())
			{
				std::memcpy(cpu + handle_size, ray_gen.local_data.data(), ray_gen.local_data.size());
			}
			raygen_region.deviceAddress = addr;
			raygen_region.stride        = raygen_region_size; // raygen stride == region size per spec
			raygen_region.size          = raygen_region_size;
			cpu  += raygen_region_size;
			addr += raygen_region_size;
		}

		auto WriteRegion = [&](std::vector<Record> const& records, Uint32 stride, Uint64 region_size, VkStridedDeviceAddressRegionKHR& out)
		{
			out.deviceAddress = records.empty() ? 0 : addr;
			out.stride        = stride;
			out.size          = (VkDeviceSize)((Uint64)stride * records.size());
			for (Uint64 i = 0; i < records.size(); ++i)
			{
				Record const& r = records[i];
				void const* handle = pipeline->GetShaderGroupHandle(r.name.c_str());
				ADRIA_ASSERT(handle != nullptr);
				Uint8* dst = cpu + i * stride;
				std::memcpy(dst, handle, handle_size);
				if (!r.local_data.empty())
				{
					std::memcpy(dst + handle_size, r.local_data.data(), r.local_data.size());
				}
			}
			cpu  += region_size;
			addr += region_size;
		};

		WriteRegion(miss,     miss_stride,     miss_region_size,     miss_region);
		WriteRegion(hit,      hit_stride,      hit_region_size,      hit_region);
		WriteRegion(callable, callable_stride, callable_region_size, callable_region);
	}
}
