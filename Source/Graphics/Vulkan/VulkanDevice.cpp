// VMA implementation — must be in exactly one translation unit
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "VulkanDevice.h"
#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "VulkanSwapchain.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanFence.h"
#include "VulkanQueryHeap.h"
#include "VulkanPipelineState.h"
#include "VulkanConversions.h"
#include "VulkanRayTracingAS.h"
#include "VulkanRayTracingPipeline.h"
#include "Graphics/GfxCommandListPool.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDynamicAllocator.h"
#include "Platform/Window.h"
#include "Core/CommandLineOptions.h"
#include "Logging/Log.h"

#if defined(ADRIA_PLATFORM_WINDOWS)
#include <vulkan/vulkan_win32.h>
#elif defined(ADRIA_PLATFORM_LINUX)
#include <vulkan/vulkan_xcb.h>
#elif defined(ADRIA_PLATFORM_MACOS)
#include <vulkan/vulkan_metal.h>
#endif

namespace adria
{
	ADRIA_LOG_CHANNEL(Graphics);

	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT,
		VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
		void*)
	{
		if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			ADRIA_LOG(ERROR, "[Vulkan] %s", callback_data->pMessage);
		}
		else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			ADRIA_LOG(WARNING, "[Vulkan] %s", callback_data->pMessage);
		}
		return VK_FALSE;
	}

	VulkanDevice::VulkanDevice(Window* window)
	{
		window_handle = window->Handle();
		width  = window->Width();
		height = window->Height();

		CreateInstance();
		SelectPhysicalDevice();
		CreateLogicalDevice();
		CreateVmaAllocator();

		VkSurfaceKHR surface = CreateSurface();

		CreateBindlessDescriptors();
		CreateStaticSamplers();
		CreateCommonPipelineLayout();

		swapchain = std::make_unique<VulkanSwapchain>(this, surface, width, height);

		CreateCommandListPools();
		CreateDynamicAllocators();

		frame_fence.Create(this, "FrameFence");
		release_fence.Create(this, "ReleaseFence");
		graphics_fence.Create(this, "GraphicsFence");
		compute_fence.Create(this, "ComputeFence");
		copy_fence.Create(this, "CopyFence");

		capabilities.Initialize(this);

		ADRIA_LOG(INFO, "VulkanDevice initialized");
	}

	VulkanDevice::~VulkanDevice()
	{
		WaitForGPU();
		ProcessReleaseQueue();

		dynamic_allocators.clear();
		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			graphics_cmd_list_pool[i].reset();
			compute_cmd_list_pool[i].reset();
			copy_cmd_list_pool[i].reset();
		}

		swapchain.reset();

		vkDestroyPipelineLayout(device, common_pipeline_layout, nullptr);

		if (sampler_set != VK_NULL_HANDLE) { vkFreeDescriptorSets(device, sampler_pool, 1, &sampler_set); }
		for (auto& s : static_samplers) { if (s) { vkDestroySampler(device, s, nullptr); } }
		if (sampler_pool)   { vkDestroyDescriptorPool(device, sampler_pool, nullptr); }
		if (sampler_layout) { vkDestroyDescriptorSetLayout(device, sampler_layout, nullptr); }

		if (bindless_set != VK_NULL_HANDLE) { vkFreeDescriptorSets(device, bindless_pool, 1, &bindless_set); }
		if (bindless_pool)   { vkDestroyDescriptorPool(device, bindless_pool, nullptr); }
		if (bindless_layout) { vkDestroyDescriptorSetLayout(device, bindless_layout, nullptr); }

		if (vma_allocator) { vmaDestroyAllocator(vma_allocator); }

		graphics_queue.reset();
		compute_queue.reset();
		copy_queue.reset();

		frame_fence.~VulkanFence();
		release_fence.~VulkanFence();
		graphics_fence.~VulkanFence();
		compute_fence.~VulkanFence();
		copy_fence.~VulkanFence();

		vkDestroyDevice(device, nullptr);

		if (debug_messenger)
		{
			auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			if (fn) { fn(instance, debug_messenger, nullptr); }
		}
		vkDestroyInstance(instance, nullptr);
	}

	void VulkanDevice::CreateInstance()
	{
		VkApplicationInfo app_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.pApplicationName   = "Adria";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName        = "Adria Engine";
		app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion         = VK_API_VERSION_1_3;

		std::vector<Char const*> extensions = 
		{
			VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(ADRIA_PLATFORM_WINDOWS)
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(ADRIA_PLATFORM_LINUX)
			VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(ADRIA_PLATFORM_MACOS)
			VK_EXT_METAL_SURFACE_EXTENSION_NAME,
			VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		};

		std::vector<Char const*> layers;
		Bool const enable_validation = CommandLineOptions::GetDebugDevice();
		if (enable_validation)
		{
			layers.push_back("VK_LAYER_KHRONOS_validation");
		}

		VkInstanceCreateInfo ci{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		ci.pApplicationInfo        = &app_info;
		ci.enabledExtensionCount   = (Uint32)extensions.size();
		ci.ppEnabledExtensionNames = extensions.data();
		ci.enabledLayerCount       = (Uint32)layers.size();
		ci.ppEnabledLayerNames     = layers.data();
#if defined(ADRIA_PLATFORM_MACOS)
		ci.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
		VK_CHECK(vkCreateInstance(&ci, nullptr, &instance));
		if (enable_validation)
		{
			auto create_fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (create_fn)
			{
				VkDebugUtilsMessengerCreateInfoEXT dbg_ci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
				dbg_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
				dbg_ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
				dbg_ci.pfnUserCallback = VulkanDebugCallback;
				create_fn(instance, &dbg_ci, nullptr, &debug_messenger);
			}
		}

		pfn_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
			vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
	}

	void VulkanDevice::SelectPhysicalDevice()
	{
		Uint32 count = 0;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);
		ADRIA_ASSERT(count > 0);

		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(instance, &count, devices.data());

		for (auto pd : devices)
		{
			VkPhysicalDeviceProperties props{};
			vkGetPhysicalDeviceProperties(pd, &props);
			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physical_device = pd;
				break;
			}
		}
		if (physical_device == VK_NULL_HANDLE)
		{
			physical_device = devices[0];
		}

		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(physical_device, &props);
		device_name = props.deviceName;
		ADRIA_LOG(INFO, "Selected GPU: %s", props.deviceName);

		switch (props.vendorID)
		{
		case 0x10DE: vendor = GfxVendor::Nvidia; break;
		case 0x1002: vendor = GfxVendor::AMD;    break;
		case 0x8086: vendor = GfxVendor::Intel;  break;
		case 0x106B: vendor = GfxVendor::Apple;  break;
		default:     vendor = GfxVendor::Unknown; break;
		}
	}

	void VulkanDevice::CreateLogicalDevice()
	{
		Uint32 family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, nullptr);
		std::vector<VkQueueFamilyProperties> families(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, families.data());

		for (Uint32 i = 0; i < family_count; ++i)
		{
			if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && graphics_family == UINT32_MAX)
			{
				graphics_family = i;
			}
			if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && compute_family == UINT32_MAX)
			{
				compute_family = i;
			}
			if ((families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && !(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && copy_family == UINT32_MAX)
			{
				copy_family = i;
			}
		}
		if (compute_family == UINT32_MAX) { compute_family = graphics_family; }
		if (copy_family    == UINT32_MAX) { copy_family    = graphics_family; }

		Float queue_priority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queue_cis;
		std::set<Uint32> unique_families = { graphics_family, compute_family, copy_family };
		for (Uint32 fam : unique_families)
		{
			VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			qci.queueFamilyIndex = fam;
			qci.queueCount       = 1;
			qci.pQueuePriorities = &queue_priority;
			queue_cis.push_back(qci);
		}

		std::vector<Char const*> extensions = 
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

		Uint32 ext_count = 0;
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, nullptr);
		std::vector<VkExtensionProperties> available_exts(ext_count);
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, available_exts.data());

		Bool has_mesh_shader = false;
		Bool has_portability_subset = false;
		Bool has_accel_structure = false;
		Bool has_rt_pipeline = false;
		Bool has_deferred_host_ops = false;
		for (auto const& ext : available_exts)
		{
			if (strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0)
			{
				has_mesh_shader = true;
			}
			if (strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0)
			{
				has_portability_subset = true;
			}
			if (strcmp(ext.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0)
			{
				has_accel_structure = true;
			}
			if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0)
			{
				has_rt_pipeline = true;
			}
			if (strcmp(ext.extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0)
			{
				has_deferred_host_ops = true;
			}
		}
		if (has_mesh_shader)
		{
			extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
		}
		if (has_portability_subset)
		{
			extensions.push_back("VK_KHR_portability_subset");
		}

		Bool rt_extensions_available = has_accel_structure && has_rt_pipeline && has_deferred_host_ops;
		VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features_query{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR    rt_features_query{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
		if (rt_extensions_available)
		{
			as_features_query.pNext = &rt_features_query;
			VkPhysicalDeviceFeatures2 probe{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
			probe.pNext = &as_features_query;
			vkGetPhysicalDeviceFeatures2(physical_device, &probe);

			rt_supported = as_features_query.accelerationStructure && rt_features_query.rayTracingPipeline;
		}
		if (rt_supported)
		{
			extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
			extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
			extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		}

		VkPhysicalDeviceFeatures supported_features{};
		vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

		VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		features13.dynamicRendering               = VK_TRUE;
		features13.synchronization2               = VK_TRUE;
		features13.maintenance4                   = VK_TRUE;
		features13.shaderDemoteToHelperInvocation  = VK_TRUE;

		VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		features12.pNext                                  = &features13;
		features12.bufferDeviceAddress                               = VK_TRUE;
		features12.scalarBlockLayout                                  = VK_TRUE;
		features12.descriptorIndexing                                 = VK_TRUE;
		features12.descriptorBindingPartiallyBound                    = VK_TRUE;
		features12.descriptorBindingSampledImageUpdateAfterBind       = VK_TRUE;
		features12.descriptorBindingStorageImageUpdateAfterBind       = VK_TRUE;
		features12.descriptorBindingStorageBufferUpdateAfterBind      = VK_TRUE;
		features12.descriptorBindingUniformBufferUpdateAfterBind      = VK_TRUE;
		features12.runtimeDescriptorArray                             = VK_TRUE;
		features12.shaderSampledImageArrayNonUniformIndexing          = VK_TRUE;
		features12.shaderStorageImageArrayNonUniformIndexing          = VK_TRUE;
		features12.shaderStorageBufferArrayNonUniformIndexing         = VK_TRUE;
		features12.timelineSemaphore                                  = VK_TRUE;
		features12.drawIndirectCount                                  = VK_FALSE;
		features12.hostQueryReset                                     = VK_TRUE;

		VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
		if (has_mesh_shader)
		{
			mesh_features.pNext      = &features12;
			mesh_features.meshShader = VK_TRUE;
			mesh_features.taskShader = VK_TRUE;
		}

		VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features_enable{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR    rt_features_enable{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
		if (rt_supported)
		{
			as_features_enable.accelerationStructure = VK_TRUE;
			as_features_enable.accelerationStructureCaptureReplay   = as_features_query.accelerationStructureCaptureReplay;
			as_features_enable.accelerationStructureIndirectBuild   = as_features_query.accelerationStructureIndirectBuild;
			as_features_enable.accelerationStructureHostCommands    = as_features_query.accelerationStructureHostCommands;
			as_features_enable.descriptorBindingAccelerationStructureUpdateAfterBind = as_features_query.descriptorBindingAccelerationStructureUpdateAfterBind;

			rt_features_enable.rayTracingPipeline = VK_TRUE;
			rt_features_enable.rayTracingPipelineTraceRaysIndirect     = rt_features_query.rayTracingPipelineTraceRaysIndirect;
			rt_features_enable.rayTracingPipelineShaderGroupHandleCaptureReplay = rt_features_query.rayTracingPipelineShaderGroupHandleCaptureReplay;
			rt_features_enable.rayTraversalPrimitiveCulling            = rt_features_query.rayTraversalPrimitiveCulling;

			as_features_enable.pNext = &rt_features_enable;
			rt_features_enable.pNext = &features13;
			features12.pNext = &as_features_enable;
		}

		VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features2.pNext = has_mesh_shader ? (void*)&mesh_features : (void*)&features12;
		features2.features.multiDrawIndirect          = supported_features.multiDrawIndirect;
		features2.features.fillModeNonSolid           = supported_features.fillModeNonSolid;
		features2.features.samplerAnisotropy          = supported_features.samplerAnisotropy;
		features2.features.shaderInt64                = supported_features.shaderInt64;
		features2.features.shaderClipDistance          = supported_features.shaderClipDistance;
		features2.features.fragmentStoresAndAtomics   = supported_features.fragmentStoresAndAtomics;
		features2.features.tessellationShader         = supported_features.tessellationShader;
		features2.features.geometryShader             = supported_features.geometryShader;
		features2.features.shaderStorageImageExtendedFormats = supported_features.shaderStorageImageExtendedFormats;

		VkDeviceCreateInfo ci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		ci.pNext                   = &features2;
		ci.queueCreateInfoCount    = (Uint32)queue_cis.size();
		ci.pQueueCreateInfos       = queue_cis.data();
		ci.enabledExtensionCount   = (Uint32)extensions.size();
		ci.ppEnabledExtensionNames = extensions.data();

		VK_CHECK(vkCreateDevice(physical_device, &ci, nullptr, &device));

		pfn_vkSetDebugUtilsObjectNameEXT  = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
		pfn_vkCmdBeginDebugUtilsLabelEXT  = (PFN_vkCmdBeginDebugUtilsLabelEXT) vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
		pfn_vkCmdEndDebugUtilsLabelEXT    = (PFN_vkCmdEndDebugUtilsLabelEXT)   vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");
		pfn_vkCmdDrawMeshTasksEXT         = (PFN_vkCmdDrawMeshTasksEXT)        vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT");
		pfn_vkCmdDrawMeshTasksIndirectEXT = (PFN_vkCmdDrawMeshTasksIndirectEXT)vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectEXT");

		if (rt_supported)
		{
			pfn_vkCreateAccelerationStructureKHR              = (PFN_vkCreateAccelerationStructureKHR)              vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
			pfn_vkDestroyAccelerationStructureKHR             = (PFN_vkDestroyAccelerationStructureKHR)             vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
			pfn_vkGetAccelerationStructureBuildSizesKHR       = (PFN_vkGetAccelerationStructureBuildSizesKHR)       vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
			pfn_vkGetAccelerationStructureDeviceAddressKHR    = (PFN_vkGetAccelerationStructureDeviceAddressKHR)    vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
			pfn_vkCmdBuildAccelerationStructuresKHR           = (PFN_vkCmdBuildAccelerationStructuresKHR)           vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
			pfn_vkCmdWriteAccelerationStructuresPropertiesKHR = (PFN_vkCmdWriteAccelerationStructuresPropertiesKHR) vkGetDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR");
			pfn_vkCmdCopyAccelerationStructureKHR             = (PFN_vkCmdCopyAccelerationStructureKHR)             vkGetDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR");
			pfn_vkCreateRayTracingPipelinesKHR                = (PFN_vkCreateRayTracingPipelinesKHR)                vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
			pfn_vkGetRayTracingShaderGroupHandlesKHR          = (PFN_vkGetRayTracingShaderGroupHandlesKHR)          vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
			pfn_vkCmdTraceRaysKHR                             = (PFN_vkCmdTraceRaysKHR)                             vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");

			//If any required entry point failed to load the driver is misbehaving disable RT rather than crash at first use
			if (!pfn_vkCreateAccelerationStructureKHR || !pfn_vkGetAccelerationStructureBuildSizesKHR ||
				!pfn_vkCmdBuildAccelerationStructuresKHR || !pfn_vkCreateRayTracingPipelinesKHR ||
				!pfn_vkGetRayTracingShaderGroupHandlesKHR || !pfn_vkCmdTraceRaysKHR)
			{
				ADRIA_LOG(WARNING, "Ray tracing extensions enabled but required entry points failed to load; disabling RT.");
				rt_supported = false;
			}
		}

		if (rt_supported)
		{
			rt_pipeline_props.pNext = &as_props;
			VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			props2.pNext = &rt_pipeline_props;
			vkGetPhysicalDeviceProperties2(physical_device, &props2);
			rt_pipeline_props.pNext = nullptr;
			as_props.pNext          = nullptr;

			ADRIA_LOG(INFO, "Ray tracing enabled (shaderGroupHandleSize=%u, shaderGroupBaseAlignment=%u, maxRecursionDepth=%u)",
				rt_pipeline_props.shaderGroupHandleSize,
				rt_pipeline_props.shaderGroupBaseAlignment,
				rt_pipeline_props.maxRayRecursionDepth);
		}

		VkQueue gfx_q = VK_NULL_HANDLE, cmp_q = VK_NULL_HANDLE, cpy_q = VK_NULL_HANDLE;
		vkGetDeviceQueue(device, graphics_family, 0, &gfx_q);
		vkGetDeviceQueue(device, compute_family,  0, &cmp_q);
		vkGetDeviceQueue(device, copy_family,     0, &cpy_q);

		graphics_queue = std::make_unique<VulkanCommandQueue>(device, gfx_q, GfxCommandListType::Graphics, graphics_family);
		compute_queue  = std::make_unique<VulkanCommandQueue>(device, cmp_q, GfxCommandListType::Compute,  compute_family);
		copy_queue     = std::make_unique<VulkanCommandQueue>(device, cpy_q, GfxCommandListType::Copy,     copy_family);

		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(physical_device, &props);
		Uint64 ns_per_tick = (Uint64)props.limits.timestampPeriod;
		Uint64 freq = ns_per_tick > 0 ? (1000000000ULL / ns_per_tick) : 1000000000ULL;
		graphics_queue->SetTimestampFrequency(freq);
		compute_queue->SetTimestampFrequency(freq);
		copy_queue->SetTimestampFrequency(freq);
	}

	void VulkanDevice::CreateVmaAllocator()
	{
		VmaAllocatorCreateInfo ci{};
		ci.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		ci.physicalDevice   = physical_device;
		ci.device           = device;
		ci.instance         = instance;
		ci.vulkanApiVersion = VK_API_VERSION_1_3;
		VK_CHECK(vmaCreateAllocator(&ci, &vma_allocator));
	}

	VkSurfaceKHR VulkanDevice::CreateSurface()
	{
		VkSurfaceKHR surface = VK_NULL_HANDLE;
#if defined(ADRIA_PLATFORM_WINDOWS)
		VkWin32SurfaceCreateInfoKHR ci{ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
		ci.hinstance = GetModuleHandle(nullptr);
		ci.hwnd      = (HWND)window_handle;
		VK_CHECK(vkCreateWin32SurfaceKHR(instance, &ci, nullptr, &surface));
#elif defined(ADRIA_PLATFORM_LINUX)
		// window_handle is expected to be a xcb_connection_t* packed alongside xcb_window_t
		// Platform/Linux/Window.cpp will store {connection, window} in a struct
		struct XcbWindowHandle { void* connection; Uint32 window; };
		XcbWindowHandle* xcb = (XcbWindowHandle*)window_handle;
		VkXcbSurfaceCreateInfoKHR ci{ VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR };
		ci.connection = (xcb_connection_t*)xcb->connection;
		ci.window     = xcb->window;
		VK_CHECK(vkCreateXcbSurfaceKHR(instance, &ci, nullptr, &surface));
#elif defined(ADRIA_PLATFORM_MACOS)
		VkMetalSurfaceCreateInfoEXT ci{ VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT };
		ci.pLayer = (CAMetalLayer*)window_handle;
		VK_CHECK(vkCreateMetalSurfaceEXT(instance, &ci, nullptr, &surface));
#endif
		return surface;
	}

	void VulkanDevice::CreateBindlessDescriptors()
	{
		std::vector<VkDescriptorPoolSize> pool_sizes = 
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_BINDLESS_MAX_COUNT },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_BINDLESS_MAX_COUNT },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_BINDLESS_MAX_COUNT },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_BINDLESS_MAX_COUNT },
		};
		if (rt_supported)
		{
			pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_BINDLESS_MAX_AS_COUNT });
		}

		VkDescriptorPoolCreateInfo pool_ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_ci.maxSets       = 1;
		pool_ci.poolSizeCount = (Uint32)pool_sizes.size();
		pool_ci.pPoolSizes    = pool_sizes.data();
		VK_CHECK(vkCreateDescriptorPool(device, &pool_ci, nullptr, &bindless_pool));

		Uint32 const binding_count = rt_supported ? VK_BINDLESS_BINDING_COUNT : (VK_BINDLESS_BINDING_COUNT - 1);
		VkDescriptorSetLayoutBinding bindings[VK_BINDLESS_BINDING_COUNT] = {};
		VkDescriptorBindingFlags binding_flags[VK_BINDLESS_BINDING_COUNT] = {};
		VkDescriptorType types[VK_BINDLESS_BINDING_COUNT] = 
		{
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		};
		Uint32 descriptor_counts[VK_BINDLESS_BINDING_COUNT] = 
		{
			VK_BINDLESS_MAX_COUNT,
			VK_BINDLESS_MAX_COUNT,
			VK_BINDLESS_MAX_COUNT,
			VK_BINDLESS_MAX_COUNT,
			VK_BINDLESS_MAX_AS_COUNT,
		};

		for (Uint32 i = 0; i < binding_count; ++i)
		{
			bindings[i].binding         = i;
			bindings[i].descriptorType  = types[i];
			bindings[i].descriptorCount = descriptor_counts[i];
			bindings[i].stageFlags      = VK_SHADER_STAGE_ALL;

			binding_flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
				              | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
		flags_ci.bindingCount  = binding_count;
		flags_ci.pBindingFlags = binding_flags;

		VkDescriptorSetLayoutCreateInfo layout_ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_ci.pNext        = &flags_ci;
		layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		layout_ci.bindingCount = binding_count;
		layout_ci.pBindings    = bindings;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_ci, nullptr, &bindless_layout));

		VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc.descriptorPool     = bindless_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts        = &bindless_layout;
		VK_CHECK(vkAllocateDescriptorSets(device, &alloc, &bindless_set));
	}

	void VulkanDevice::CreateStaticSamplers()
	{
		struct SamplerSpec { VkFilter filter; VkSamplerAddressMode addr; VkCompareOp cmp; };
		SamplerSpec specs[10] = {
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,          VK_COMPARE_OP_NEVER      }, // s0 LinearWrap
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   VK_COMPARE_OP_NEVER      }, // s1 LinearClamp
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_COMPARE_OP_NEVER      }, // s2 LinearBorder
			{ VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT,          VK_COMPARE_OP_NEVER      }, // s3 PointWrap
			{ VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   VK_COMPARE_OP_NEVER      }, // s4 PointClamp
			{ VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_COMPARE_OP_NEVER      }, // s5 PointBorder
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   VK_COMPARE_OP_LESS_OR_EQUAL }, // s6 ShadowClamp
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_REPEAT,          VK_COMPARE_OP_LESS_OR_EQUAL }, // s7 ShadowWrap
			{ VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_COMPARE_OP_NEVER      }, // s8 LinearMirror
			{ VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT, VK_COMPARE_OP_NEVER      }, // s9 PointMirror
		};

		for (Uint32 i = 0; i < 10; ++i)
		{
			VkSamplerCreateInfo ci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			ci.magFilter               = specs[i].filter;
			ci.minFilter               = specs[i].filter;
			ci.mipmapMode              = specs[i].filter == VK_FILTER_LINEAR
			                           ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
			ci.addressModeU            = specs[i].addr;
			ci.addressModeV            = specs[i].addr;
			ci.addressModeW            = specs[i].addr;
			ci.maxLod                  = VK_LOD_CLAMP_NONE;
			ci.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			ci.anisotropyEnable        = (specs[i].filter == VK_FILTER_LINEAR && specs[i].cmp == VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
			ci.maxAnisotropy           = ci.anisotropyEnable ? 16.0f : 1.0f;
			ci.compareEnable           = (specs[i].cmp != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
			ci.compareOp               = specs[i].cmp;
			VK_CHECK(vkCreateSampler(device, &ci, nullptr, &static_samplers[i]));
		}

		VkDescriptorPoolSize pool_size{ VK_DESCRIPTOR_TYPE_SAMPLER, 10 };
		VkDescriptorPoolCreateInfo pool_ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_ci.maxSets       = 1;
		pool_ci.poolSizeCount = 1;
		pool_ci.pPoolSizes    = &pool_size;
		VK_CHECK(vkCreateDescriptorPool(device, &pool_ci, nullptr, &sampler_pool));

		VkDescriptorSetLayoutBinding sampler_bindings[10]{};
		for (Uint32 i = 0; i < 10; ++i)
		{
			sampler_bindings[i].binding         = i;
			sampler_bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
			sampler_bindings[i].descriptorCount = 1;
			sampler_bindings[i].stageFlags      = VK_SHADER_STAGE_ALL;
		}

		VkDescriptorSetLayoutCreateInfo layout_ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layout_ci.bindingCount = 10;
		layout_ci.pBindings    = sampler_bindings;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_ci, nullptr, &sampler_layout));

		VkDescriptorSetAllocateInfo alloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc.descriptorPool     = sampler_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts        = &sampler_layout;
		VK_CHECK(vkAllocateDescriptorSets(device, &alloc, &sampler_set));

		std::array<VkDescriptorImageInfo, 10> sampler_infos{};
		std::array<VkWriteDescriptorSet, 10> writes{};
		for (Uint32 i = 0; i < 10; ++i)
		{
			sampler_infos[i].sampler = static_samplers[i];
			writes[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writes[i].dstSet          = sampler_set;
			writes[i].dstBinding      = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
			writes[i].pImageInfo      = &sampler_infos[i];
		}
		vkUpdateDescriptorSets(device, 10, writes.data(), 0, nullptr);
	}

	void VulkanDevice::CreateCommonPipelineLayout()
	{
		VkDescriptorSetLayout set_layouts[2] = { bindless_layout, sampler_layout };

		VkPushConstantRange push_range{};
		push_range.stageFlags = VK_SHADER_STAGE_ALL;
		push_range.offset     = 0;
		push_range.size       = VK_PUSH_CONSTANT_SIZE;

		VkPipelineLayoutCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		ci.setLayoutCount         = 2;
		ci.pSetLayouts            = set_layouts;
		ci.pushConstantRangeCount = 1;
		ci.pPushConstantRanges    = &push_range;
		VK_CHECK(vkCreatePipelineLayout(device, &ci, nullptr, &common_pipeline_layout));
	}

	void VulkanDevice::CreateCommandListPools()
	{
		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			graphics_cmd_list_pool[i] = std::make_unique<GfxGraphicsCommandListPool>(this);
			compute_cmd_list_pool[i]  = std::make_unique<GfxComputeCommandListPool>(this);
			copy_cmd_list_pool[i]     = std::make_unique<GfxCopyCommandListPool>(this);
			graphics_cmd_list_pool[i]->BeginCmdLists();
			compute_cmd_list_pool[i]->BeginCmdLists();
			copy_cmd_list_pool[i]->BeginCmdLists();
		}
	}

	void VulkanDevice::CreateDynamicAllocators()
	{
		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			dynamic_allocators.push_back(std::make_unique<GfxLinearDynamicAllocator>(this, 8 * 1024 * 1024));
		}
	}

	void VulkanDevice::OnResize(Uint32 w, Uint32 h)
	{
		width  = w;
		height = h;
		swapchain->OnResize(w, h);
	}

	GfxTexture* VulkanDevice::GetBackbuffer() const
	{
		return swapchain->GetBackbuffer();
	}

	Uint32 VulkanDevice::GetBackbufferIndex() const
	{
		return swapchain->GetBackbufferIndex();
	}

	void VulkanDevice::Update()
	{
	}

	void VulkanDevice::BeginFrame()
	{
		frame_fence.Wait(frame_fence_values[frame_index]);
		swapchain->AcquireNextImage();
		ProcessReleaseQueue();
		dynamic_allocators[frame_index]->Clear();

		{
			std::lock_guard lock(bindless_mutex);
			auto& releases = pending_bindless_releases[frame_index];
			for (auto const& [binding, index] : releases)
			{
				bindless_free_list[binding].push_back(index);
			}
			releases.clear();
		}

		graphics_cmd_list_pool[frame_index]->BeginCmdLists();
		compute_cmd_list_pool[frame_index]->BeginCmdLists();
		copy_cmd_list_pool[frame_index]->BeginCmdLists();

		first_frame            = rendering_not_started;
		rendering_not_started  = false;
	}

	void VulkanDevice::EndFrame()
	{
		graphics_cmd_list_pool[frame_index]->EndCmdLists();
		compute_cmd_list_pool[frame_index]->EndCmdLists();
		copy_cmd_list_pool[frame_index]->EndCmdLists();

		VkSemaphore image_available = swapchain->GetImageAvailableSemaphore();
		VkSemaphore render_finished = swapchain->GetRenderFinishedSemaphore();

		VkSemaphoreSubmitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		wait_info.semaphore = image_available;
		wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo signal_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		signal_info.semaphore = render_finished;
		signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		VkSemaphoreSubmitInfo frame_signal{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		frame_signal.semaphore = frame_fence.GetSemaphore();
		frame_signal.value     = ++frame_fence_value;
		frame_signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

		VkSemaphoreSubmitInfo signals[2] = { signal_info, frame_signal };

		std::vector<VkCommandBufferSubmitInfo> cmd_infos;
		for (auto& cmd_list : *graphics_cmd_list_pool[frame_index])
		{
			VkCommandBufferSubmitInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
			info.commandBuffer = (VkCommandBuffer)cmd_list->GetNative();
			cmd_infos.push_back(info);
		}

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.waitSemaphoreInfoCount   = 1;
		submit.pWaitSemaphoreInfos      = &wait_info;
		submit.commandBufferInfoCount   = (Uint32)cmd_infos.size();
		submit.pCommandBufferInfos      = cmd_infos.data();
		submit.signalSemaphoreInfoCount = 2;
		submit.pSignalSemaphoreInfos    = signals;

		VK_CHECK(vkQueueSubmit2(graphics_queue->GetQueue(), 1, &submit, VK_NULL_HANDLE));

		frame_fence_values[frame_index] = frame_fence_value;
		swapchain->Present(false);

		frame_index = (frame_index + 1) % GFX_BACKBUFFER_COUNT;
	}

	void VulkanDevice::WaitForGPU()
	{
		vkDeviceWaitIdle(device);
	}

	GfxCommandQueue* VulkanDevice::GetCommandQueue(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_queue.get();
		case GfxCommandListType::Compute:  return compute_queue.get();
		case GfxCommandListType::Copy:     return copy_queue.get();
		default: return graphics_queue.get();
		}
	}

	GfxFence& VulkanDevice::GetFence(GfxCommandListType type)
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_fence;
		case GfxCommandListType::Compute:  return compute_fence;
		case GfxCommandListType::Copy:     return copy_fence;
		default: return graphics_fence;
		}
	}

	Uint64 VulkanDevice::GetFenceValue(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_fence_value;
		case GfxCommandListType::Compute:  return compute_fence_value;
		case GfxCommandListType::Copy:     return copy_fence_value;
		default: return 0;
		}
	}

	void VulkanDevice::SetFenceValue(GfxCommandListType type, Uint64 value)
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: graphics_fence_value = value; break;
		case GfxCommandListType::Compute:  compute_fence_value  = value; break;
		case GfxCommandListType::Copy:     copy_fence_value     = value; break;
		default: break;
		}
	}

	GfxCommandList* VulkanDevice::GetCommandList(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_cmd_list_pool[frame_index]->GetMainCmdList();
		case GfxCommandListType::Compute:  return compute_cmd_list_pool[frame_index]->GetMainCmdList();
		case GfxCommandListType::Copy:     return copy_cmd_list_pool[frame_index]->GetMainCmdList();
		default: return nullptr;
		}
	}

	GfxCommandList* VulkanDevice::GetLatestCommandList(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_cmd_list_pool[frame_index]->GetLatestCmdList();
		case GfxCommandListType::Compute:  return compute_cmd_list_pool[frame_index]->GetLatestCmdList();
		case GfxCommandListType::Copy:     return copy_cmd_list_pool[frame_index]->GetLatestCmdList();
		default: return nullptr;
		}
	}

	GfxCommandList* VulkanDevice::AllocateCommandList(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_cmd_list_pool[frame_index]->AllocateCmdList();
		case GfxCommandListType::Compute:  return compute_cmd_list_pool[frame_index]->AllocateCmdList();
		case GfxCommandListType::Copy:     return copy_cmd_list_pool[frame_index]->AllocateCmdList();
		default: return nullptr;
		}
	}

	void VulkanDevice::FreeCommandList(GfxCommandList* cmd_list, GfxCommandListType type)
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: graphics_cmd_list_pool[frame_index]->FreeCmdList(cmd_list); break;
		case GfxCommandListType::Compute:  compute_cmd_list_pool[frame_index]->FreeCmdList(cmd_list);  break;
		case GfxCommandListType::Copy:     copy_cmd_list_pool[frame_index]->FreeCmdList(cmd_list);     break;
		default: break;
		}
	}

	GfxLinearDynamicAllocator* VulkanDevice::GetDynamicAllocator() const
	{
		return dynamic_allocators[frame_index].get();
	}

	Uint32 VulkanDevice::GetBindlessDescriptorIndex(GfxDescriptor descriptor) const
	{
		return (Uint32)descriptor.opaque_data[0];
	}

	void VulkanDevice::FreeDescriptor(GfxDescriptor descriptor)
	{
		if (descriptor.opaque_data[0] != GfxDescriptor::INVALID_OPAQUE_DATA
			&& descriptor.opaque_data[1] != GfxDescriptor::INVALID_OPAQUE_DATA)
		{
			Uint32 binding = (Uint32)descriptor.opaque_data[1];
			Uint32 index   = (Uint32)descriptor.opaque_data[0];
			std::lock_guard lock(bindless_mutex);
			pending_bindless_releases[frame_index].emplace_back(binding, index);
		}
	}

	std::unique_ptr<GfxCommandList> VulkanDevice::CreateCommandList(GfxCommandListType type)
	{
		return std::make_unique<VulkanCommandList>(this, type);
	}

	std::unique_ptr<GfxTexture> VulkanDevice::CreateTexture(GfxTextureDesc const& desc)
	{
		return std::make_unique<VulkanTexture>(this, desc);
	}

	std::unique_ptr<GfxTexture> VulkanDevice::CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data)
	{
		return std::make_unique<VulkanTexture>(this, desc, data);
	}

	std::unique_ptr<GfxTexture> VulkanDevice::CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer)
	{
		return std::make_unique<VulkanTexture>(this, desc, backbuffer);
	}

	std::unique_ptr<GfxBuffer> VulkanDevice::CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& data)
	{
		auto buf = std::make_unique<VulkanBuffer>(this, desc, data);
		if (buf->GetGpuAddress())
		{
			RegisterBuffer(buf->GetGpuAddress(), buf->GetBuffer());
		}
		return buf;
	}

	std::unique_ptr<GfxBuffer> VulkanDevice::CreateBuffer(GfxBufferDesc const& desc)
	{
		return CreateBuffer(desc, {});
	}

	std::shared_ptr<GfxBuffer> VulkanDevice::CreateBufferShared(GfxBufferDesc const& desc, GfxBufferData const& data)
	{
		auto buf = std::make_shared<VulkanBuffer>(this, desc, data);
		if (buf->GetGpuAddress())
		{
			RegisterBuffer(buf->GetGpuAddress(), buf->GetBuffer());
		}
		return buf;
	}

	std::shared_ptr<GfxBuffer> VulkanDevice::CreateBufferShared(GfxBufferDesc const& desc)
	{
		return CreateBufferShared(desc, {});
	}

	std::unique_ptr<GfxPipelineState> VulkanDevice::CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc)
	{
		return std::make_unique<VulkanPipelineState>(this, desc);
	}

	std::unique_ptr<GfxPipelineState> VulkanDevice::CreateComputePipelineState(GfxComputePipelineStateDesc const& desc)
	{
		return std::make_unique<VulkanPipelineState>(this, desc);
	}

	std::unique_ptr<GfxPipelineState> VulkanDevice::CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc)
	{
		return std::make_unique<VulkanPipelineState>(this, desc);
	}

	std::unique_ptr<GfxFence> VulkanDevice::CreateFence(Char const* name)
	{
		auto fence = std::make_unique<VulkanFence>();
		fence->Create(this, name);
		return fence;
	}

	std::unique_ptr<GfxQueryHeap> VulkanDevice::CreateQueryHeap(GfxQueryHeapDesc const& desc)
	{
		return std::make_unique<VulkanQueryHeap>(this, desc);
	}

	std::unique_ptr<GfxRayTracingTLAS> VulkanDevice::CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags)
	{
		if (!rt_supported)
		{
			ADRIA_ASSERT(false && "CreateRayTracingTLAS called but ray tracing is not supported on this device");
			return nullptr;
		}
		return std::make_unique<VulkanRayTracingTLAS>(this, instances, flags);
	}

	std::unique_ptr<GfxRayTracingBLAS> VulkanDevice::CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags)
	{
		if (!rt_supported)
		{
			ADRIA_ASSERT(false && "CreateRayTracingBLAS called but ray tracing is not supported on this device");
			return nullptr;
		}
		return std::make_unique<VulkanRayTracingBLAS>(this, geometries, flags);
	}

	std::unique_ptr<GfxRayTracingPipeline> VulkanDevice::CreateRayTracingPipeline(GfxRayTracingPipelineDesc const& desc)
	{
		if (!rt_supported)
		{
			ADRIA_ASSERT(false && "CreateRayTracingPipeline called but ray tracing is not supported on this device");
			return nullptr;
		}
		return std::make_unique<VulkanRayTracingPipeline>(this, desc);
	}

	GfxDescriptor VulkanDevice::MakeBindlessDescriptor(Uint32 binding, Uint32 index) const
	{
		GfxDescriptor d{};
		d.opaque_data[0] = (Uint64)index;
		d.opaque_data[1] = (Uint64)binding;
		return d;
	}

	GfxDescriptor VulkanDevice::MakeViewDescriptor(VkImageView view) const
	{
		GfxDescriptor d{};
		d.opaque_data[0] = GfxDescriptor::INVALID_OPAQUE_DATA;
		d.opaque_data[1] = (Uint64)view;
		return d;
	}

	namespace
	{
		VkImageViewType GetSubresourceViewType(GfxTextureDesc const& tex_desc, Uint32 slice_count, Bool allow_cube)
		{
			Bool is_cube_compatible = HasAnyFlag(tex_desc.misc_flags, GfxTextureMiscFlag::TextureCube);
			if (allow_cube && is_cube_compatible && slice_count >= 6 && slice_count % 6 == 0)
			{
				return slice_count > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			}
			switch (tex_desc.type)
			{
			case GfxTextureType_1D: return slice_count > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
			case GfxTextureType_2D: return slice_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			case GfxTextureType_3D: return VK_IMAGE_VIEW_TYPE_3D;
			default:                return VK_IMAGE_VIEW_TYPE_2D;
			}
		}

		VulkanImageViewKey MakeSubresourceKey(GfxTextureDesc const& tex_desc, GfxTextureDescriptorDesc const* desc_in,
		                                      VkImageAspectFlags aspect, Bool single_mip, Bool allow_cube)
		{
			VulkanImageViewKey key{};
			key.first_mip   = desc_in ? desc_in->first_mip : 0;
			key.first_slice = desc_in ? desc_in->first_slice : 0;

			Uint32 requested_mip_count   = desc_in ? desc_in->mip_count   : Uint32(-1);
			Uint32 requested_slice_count = desc_in ? desc_in->slice_count : Uint32(-1);
			key.mip_count = (single_mip || requested_mip_count == Uint32(-1))
				? (single_mip ? 1u : std::max<Uint32>(1u, tex_desc.mip_levels - key.first_mip))
				: requested_mip_count;
			key.slice_count = (requested_slice_count == Uint32(-1))
				? std::max<Uint32>(1u, tex_desc.array_size - key.first_slice)
				: requested_slice_count;

			key.view_type = GetSubresourceViewType(tex_desc, key.slice_count, allow_cube);
			key.aspect    = aspect;
			return key;
		}
	}

	GfxDescriptor VulkanDevice::CreateTextureSRV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc_in)
	{
		VulkanTexture* vk_tex = const_cast<VulkanTexture*>(static_cast<VulkanTexture const*>(texture));
		Uint32 index = AllocateBindlessIndex(VK_BINDLESS_BINDING_SRV);

		VkImageAspectFlags aspect = GetAspectFlags(texture->GetDesc().format);
		if (aspect & VK_IMAGE_ASPECT_DEPTH_BIT)
		{
			aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		VulkanImageViewKey key = MakeSubresourceKey(texture->GetDesc(), desc_in, aspect, /*single_mip*/ false, /*allow_cube*/ true);

		VkDescriptorImageInfo img_info{};
		img_info.imageView   = vk_tex->GetView(key);
		img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet          = bindless_set;
		write.dstBinding      = VK_BINDLESS_BINDING_SRV;
		write.dstArrayElement = index;
		write.descriptorCount = 1;
		write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo      = &img_info;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		return MakeBindlessDescriptor(VK_BINDLESS_BINDING_SRV, index);
	}

	GfxDescriptor VulkanDevice::CreateTextureUAV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc_in)
	{
		VulkanTexture* vk_tex = const_cast<VulkanTexture*>(static_cast<VulkanTexture const*>(texture));
		Uint32 index = AllocateBindlessIndex(VK_BINDLESS_BINDING_UAV);

		VulkanImageViewKey key = MakeSubresourceKey(texture->GetDesc(), desc_in, VK_IMAGE_ASPECT_COLOR_BIT, /*single_mip*/ true, /*allow_cube*/ false);

		VkImageView view = vk_tex->GetView(key);
		if (view != VK_NULL_HANDLE)
		{
			VkDescriptorImageInfo img_info{};
			img_info.imageView   = view;
			img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write.dstSet          = bindless_set;
			write.dstBinding      = VK_BINDLESS_BINDING_UAV;
			write.dstArrayElement = index;
			write.descriptorCount = 1;
			write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			write.pImageInfo      = &img_info;
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}

		return MakeBindlessDescriptor(VK_BINDLESS_BINDING_UAV, index);
	}

	GfxDescriptor VulkanDevice::CreateTextureRTV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc_in)
	{
		VulkanTexture* vk_tex = const_cast<VulkanTexture*>(static_cast<VulkanTexture const*>(texture));
		VulkanImageViewKey key = MakeSubresourceKey(texture->GetDesc(), desc_in, VK_IMAGE_ASPECT_COLOR_BIT, /*single_mip*/ true, /*allow_cube*/ false);
		return MakeViewDescriptor(vk_tex->GetView(key));
	}

	GfxDescriptor VulkanDevice::CreateTextureDSV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc_in)
	{
		VulkanTexture* vk_tex = const_cast<VulkanTexture*>(static_cast<VulkanTexture const*>(texture));
		VkImageAspectFlags aspect = GetAspectFlags(texture->GetDesc().format);
		VulkanImageViewKey key = MakeSubresourceKey(texture->GetDesc(), desc_in, aspect, /*single_mip*/ true, /*allow_cube*/ false);
		return MakeViewDescriptor(vk_tex->GetView(key));
	}

	GfxDescriptor VulkanDevice::CreateBufferSRV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc_in)
	{
		VulkanBuffer const* vk_buf = static_cast<VulkanBuffer const*>(buffer);
		Uint32 index = AllocateBindlessIndex(VK_BINDLESS_BINDING_BUF);

		VkDescriptorBufferInfo buf_info{};
		buf_info.buffer = vk_buf->GetBuffer();
		buf_info.offset = desc_in ? desc_in->offset : 0;
		Uint64 max_range = buffer->GetSize() - buf_info.offset;
		buf_info.range  = (desc_in && desc_in->size != Uint64(-1) && desc_in->size <= max_range) ? desc_in->size : max_range;

		VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet          = bindless_set;
		write.dstBinding      = VK_BINDLESS_BINDING_BUF;
		write.dstArrayElement = index;
		write.descriptorCount = 1;
		write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo     = &buf_info;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		return MakeBindlessDescriptor(VK_BINDLESS_BINDING_BUF, index);
	}

	GfxDescriptor VulkanDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc_in)
	{
		VulkanBuffer const* vk_buf = static_cast<VulkanBuffer const*>(buffer);
		Uint32 index = AllocateBindlessIndex(VK_BINDLESS_BINDING_RBUF);

		VkDescriptorBufferInfo buf_info{};
		buf_info.buffer = vk_buf->GetBuffer();
		buf_info.offset = desc_in ? desc_in->offset : 0;
		Uint64 max_range = buffer->GetSize() - buf_info.offset;
		buf_info.range  = (desc_in && desc_in->size != Uint64(-1) && desc_in->size <= max_range) ? desc_in->size : max_range;

		VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet          = bindless_set;
		write.dstBinding      = VK_BINDLESS_BINDING_RBUF;
		write.dstArrayElement = index;
		write.descriptorCount = 1;
		write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo     = &buf_info;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		return MakeBindlessDescriptor(VK_BINDLESS_BINDING_RBUF, index);
	}

	GfxDescriptor VulkanDevice::CreateBufferUAV(GfxBuffer const* buffer, GfxBuffer const*, GfxBufferDescriptorDesc const* desc_in)
	{
		return CreateBufferUAV(buffer, desc_in);
	}

	GfxDescriptor VulkanDevice::CreateRayTracingTLASSRV(GfxRayTracingTLAS const* tlas)
	{
		ADRIA_ASSERT(rt_supported && "CreateRayTracingTLASSRV called but ray tracing is not supported");
		ADRIA_ASSERT(tlas != nullptr);

		Uint32 index = AllocateBindlessIndex(VK_BINDLESS_BINDING_AS);

		VkAccelerationStructureKHR vk_tlas = static_cast<VulkanRayTracingTLAS const*>(tlas)->GetVkHandle();
		VkWriteDescriptorSetAccelerationStructureKHR as_write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		as_write.accelerationStructureCount = 1;
		as_write.pAccelerationStructures    = &vk_tlas;

		VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.pNext            = &as_write;
		write.dstSet           = bindless_set;
		write.dstBinding       = VK_BINDLESS_BINDING_AS;
		write.dstArrayElement  = index;
		write.descriptorCount  = 1;
		write.descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		return MakeBindlessDescriptor(VK_BINDLESS_BINDING_AS, index);
	}

	Uint64 VulkanDevice::GetLinearBufferSize(GfxTexture const* texture) const
	{
		VkMemoryRequirements req{};
		VulkanTexture const* vk_tex = static_cast<VulkanTexture const*>(texture);
		vkGetImageMemoryRequirements(device, vk_tex->GetImage(), &req);
		return req.size;
	}

	Uint64 VulkanDevice::GetLinearBufferSize(GfxBuffer const* buffer) const
	{
		return buffer->GetSize();
	}

	void VulkanDevice::GetTimestampFrequency(Uint64& frequency) const
	{
		frequency = graphics_queue->GetTimestampFrequency();
	}

	GPUMemoryUsage VulkanDevice::GetMemoryUsage() const
	{
		VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
		vmaGetHeapBudgets(vma_allocator, budgets);
		Uint64 usage = 0, budget_total = 0;
		for (Uint32 i = 0; i < VK_MAX_MEMORY_HEAPS; ++i)
		{
			usage        += budgets[i].usage;
			budget_total += budgets[i].budget;
		}
		return { usage, budget_total };
	}

	Uint32 VulkanDevice::GetQueueFamilyIndex(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_family;
		case GfxCommandListType::Compute:  return compute_family;
		case GfxCommandListType::Copy:     return copy_family;
		default: return graphics_family;
		}
	}

	VulkanBufferLookup VulkanDevice::GetBufferFromAddress(VkDeviceAddress address) const
	{
		std::lock_guard lock(bda_map_mutex);
		auto it = bda_to_buffer.upper_bound(address);
		if (it != bda_to_buffer.begin())
		{
			--it;
			return { it->second, address - it->first };
		}
		ADRIA_ASSERT(false && "VkBuffer not found for given device address");
		return {};
	}

	void VulkanDevice::TransitionImageLayoutImmediate(VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout)
	{
		VkCommandPoolCreateInfo pool_ci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		pool_ci.queueFamilyIndex = graphics_family;
		pool_ci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		VkCommandPool pool = VK_NULL_HANDLE;
		VK_CHECK(vkCreateCommandPool(device, &pool_ci, nullptr, &pool));

		VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool        = pool;
		alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		VkCommandBuffer cb = VK_NULL_HANDLE;
		VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cb));

		VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(cb, &begin_info));

		VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		barrier.srcStageMask        = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		barrier.srcAccessMask       = VK_ACCESS_2_NONE;
		barrier.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		barrier.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		barrier.oldLayout           = old_layout;
		barrier.newLayout           = new_layout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image               = image;
		barrier.subresourceRange    = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

		VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dep.imageMemoryBarrierCount = 1;
		dep.pImageMemoryBarriers    = &barrier;
		vkCmdPipelineBarrier2(cb, &dep);

		VK_CHECK(vkEndCommandBuffer(cb));

		VkCommandBufferSubmitInfo cb_submit{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cb_submit.commandBuffer = cb;

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.commandBufferInfoCount = 1;
		submit.pCommandBufferInfos    = &cb_submit;

		VkQueue queue = graphics_queue->GetQueue();
		VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));
		VK_CHECK(vkQueueWaitIdle(queue));

		vkDestroyCommandPool(device, pool, nullptr);
	}

	Uint32 VulkanDevice::AllocateBindlessIndex(Uint32 binding)
	{
		ADRIA_ASSERT(binding < VK_BINDLESS_BINDING_COUNT);
		std::lock_guard lock(bindless_mutex);
		if (!bindless_free_list[binding].empty())
		{
			Uint32 idx = bindless_free_list[binding].back();
			bindless_free_list[binding].pop_back();
			return idx;
		}
		Uint32 idx = next_bindless_index[binding].fetch_add(1, std::memory_order_relaxed);
		ADRIA_ASSERT(idx < VK_BINDLESS_MAX_COUNT && "Bindless descriptor heap exhausted");
		return idx;
	}

	void VulkanDevice::FreeBindlessIndex(Uint32 binding, Uint32 index)
	{
		ADRIA_ASSERT(binding < VK_BINDLESS_BINDING_COUNT);
		std::lock_guard lock(bindless_mutex);
		bindless_free_list[binding].push_back(index);
	}

	void VulkanDevice::RegisterBuffer(VkDeviceAddress address, VkBuffer buffer)
	{
		std::lock_guard lock(bda_map_mutex);
		bda_to_buffer[address] = buffer;
	}

	void VulkanDevice::UnregisterBuffer(VkDeviceAddress address)
	{
		std::lock_guard lock(bda_map_mutex);
		bda_to_buffer.erase(address);
	}

	void VulkanDevice::AddToReleaseQueue_Internal(ReleasableObject* obj)
	{
		release_queue.emplace(obj, release_fence_value);
		release_fence.Signal(++release_fence_value);
	}

	void VulkanDevice::ProcessReleaseQueue()
	{
		Uint64 completed = release_fence.GetCompletedValue();
		while (!release_queue.empty() && release_queue.front().fence_value <= completed)
		{
			release_queue.pop();
		}
	}
}
