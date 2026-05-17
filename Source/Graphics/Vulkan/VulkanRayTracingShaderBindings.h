#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxRayTracingShaderBindings.h"
#include <string>
#include <vector>

namespace adria
{
	class GfxLinearDynamicAllocator;
	class VulkanRayTracingPipeline;

	class VulkanRayTracingShaderBindings final : public GfxRayTracingShaderBindings
	{
	public:
		explicit VulkanRayTracingShaderBindings(VulkanRayTracingPipeline const* pipeline);
		virtual ~VulkanRayTracingShaderBindings() override = default;

		virtual void                 SetRayGenShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
		virtual GfxShaderGroupHandle AddMissShader  (Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
		virtual GfxShaderGroupHandle AddHitGroup    (Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
		virtual GfxShaderGroupHandle AddCallableShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;

		virtual void Commit() override;

		virtual Uint32 GetMissShaderIndex    (GfxShaderGroupHandle h) const override { return h.index; }
		virtual Uint32 GetHitGroupIndex      (GfxShaderGroupHandle h) const override { return h.index; }
		virtual Uint32 GetCallableShaderIndex(GfxShaderGroupHandle h) const override { return h.index; }

		void Build(GfxLinearDynamicAllocator& allocator,
			VkStridedDeviceAddressRegionKHR& raygen_region,
			VkStridedDeviceAddressRegionKHR& miss_region,
			VkStridedDeviceAddressRegionKHR& hit_region,
			VkStridedDeviceAddressRegionKHR& callable_region);

	private:
		struct Record
		{
			std::string name;
			std::vector<Uint8> local_data; 
		};

		VulkanRayTracingPipeline const* pipeline = nullptr;
		Record ray_gen;
		std::vector<Record> miss;
		std::vector<Record> hit;
		std::vector<Record> callable;
		Uint32 miss_record_size     = 0;
		Uint32 hit_record_size      = 0;
		Uint32 callable_record_size = 0;
		Bool   committed            = false;
	};
}
