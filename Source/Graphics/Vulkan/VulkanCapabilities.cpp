#include "VulkanCapabilities.h"
#include "VulkanDevice.h"

namespace adria
{
	Bool VulkanCapabilities::Initialize(GfxDevice* gfx)
	{
		VulkanDevice* vk_device = static_cast<VulkanDevice*>(gfx);
		VkPhysicalDevice physical_device = vk_device->GetVkPhysicalDevice();

		VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
		VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		VkPhysicalDeviceFragmentShadingRateFeaturesKHR vrs_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR };
		VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

		mesh_features.pNext    = &rt_features;
		rt_features.pNext      = &as_features;
		as_features.pNext      = &vrs_features;
		vrs_features.pNext     = nullptr;
		features2.pNext        = &mesh_features;

		vkGetPhysicalDeviceFeatures2(physical_device, &features2);

		mesh_shaders_supported              = mesh_features.meshShader && mesh_features.taskShader;
		hardware_ray_tracing_supported      = vk_device->IsRayTracingSupported();
		inline_ray_tracing_supported        = false;
		variable_rate_shading_supported     = vrs_features.pipelineFragmentShadingRate;
		variable_rate_shading_image_supported = vrs_features.attachmentFragmentShadingRate;
		work_graphs_supported               = false;
		enhanced_barriers_supported         = true; // sync2 is core in 1.3
		typed_uav_additional_formats_supported = true;
		shader_model                        = SM_6_6;

		if (variable_rate_shading_image_supported)
		{
			VkPhysicalDeviceFragmentShadingRatePropertiesKHR vrs_props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR };
			VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			props2.pNext = &vrs_props;
			vkGetPhysicalDeviceProperties2(physical_device, &props2);
			shading_rate_image_tile_size = vrs_props.minFragmentShadingRateAttachmentTexelSize.width;
		}

		return true;
	}
}
