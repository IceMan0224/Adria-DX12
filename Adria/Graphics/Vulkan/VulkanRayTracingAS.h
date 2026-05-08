#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxRayTracingAS.h"
#include <memory>
#include <vector>

namespace adria
{
	class GfxBuffer;
	class GfxDevice;
	class VulkanDevice;
	class VulkanCommandList;

	class VulkanRayTracingBLAS final : public GfxRayTracingBLAS
	{
		friend class VulkanCommandList;
	public:
		VulkanRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
		virtual ~VulkanRayTracingBLAS() override;

		virtual Uint64 GetGpuAddress() const override { return as_device_address; }
		virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

	private:
		VulkanDevice* vk_device = nullptr;
		VkAccelerationStructureKHR as = VK_NULL_HANDLE;
		VkDeviceAddress as_device_address = 0;
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::vector<VkAccelerationStructureGeometryKHR>       geometries_vk;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_ranges;
		std::vector<Uint32>                                   primitive_counts;
		VkBuildAccelerationStructureFlagsKHR build_flags = 0;
	};

	class VulkanRayTracingTLAS final : public GfxRayTracingTLAS
	{
		friend class VulkanCommandList;
	public:
		VulkanRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
		virtual ~VulkanRayTracingTLAS() override;

		virtual Uint64 GetGpuAddress() const override { return as_device_address; }
		virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }
		virtual void UpdateInstances(std::span<GfxRayTracingInstance> instances) override;

		VkAccelerationStructureKHR GetVkHandle() const { return as; }

	private:
		VulkanDevice* vk_device = nullptr;
		VkAccelerationStructureKHR as = VK_NULL_HANDLE;
		VkDeviceAddress as_device_address = 0;
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::unique_ptr<GfxBuffer> instance_buffer;
		Uint32 instance_count = 0;
		VkBuildAccelerationStructureFlagsKHR build_flags = 0;
	};
}
