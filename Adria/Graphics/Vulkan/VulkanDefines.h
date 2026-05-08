#pragma once
#include <vulkan/vulkan.h>
#include "Core/FatalAssert.h"

#define VK_CHECK(x) \
	do { \
		VkResult _vk_result = (x); \
		ADRIA_FATAL_ASSERT(_vk_result == VK_SUCCESS, "Vulkan error %d in " #x, (int)_vk_result); \
	} while(0)

#define VK_OBJECT_SET_NAME(device, handle, type, name) \
	do { \
		if (pfn_vkSetDebugUtilsObjectNameEXT) { \
			VkDebugUtilsObjectNameInfoEXT _name_info{}; \
			_name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT; \
			_name_info.objectType = (type); \
			_name_info.objectHandle = (uint64_t)(handle); \
			_name_info.pObjectName = (name); \
			pfn_vkSetDebugUtilsObjectNameEXT((device), &_name_info); \
		} \
	} while(0)

inline PFN_vkSetDebugUtilsObjectNameEXT    pfn_vkSetDebugUtilsObjectNameEXT    = nullptr;
inline PFN_vkCmdBeginDebugUtilsLabelEXT    pfn_vkCmdBeginDebugUtilsLabelEXT    = nullptr;
inline PFN_vkCmdEndDebugUtilsLabelEXT      pfn_vkCmdEndDebugUtilsLabelEXT      = nullptr;
inline PFN_vkCmdDrawMeshTasksEXT           pfn_vkCmdDrawMeshTasksEXT           = nullptr;
inline PFN_vkCmdDrawMeshTasksIndirectEXT   pfn_vkCmdDrawMeshTasksIndirectEXT   = nullptr;

// KHR ray tracing / acceleration structure entry points (null unless RT is supported)
inline PFN_vkCreateAccelerationStructureKHR                pfn_vkCreateAccelerationStructureKHR                = nullptr;
inline PFN_vkDestroyAccelerationStructureKHR               pfn_vkDestroyAccelerationStructureKHR               = nullptr;
inline PFN_vkGetAccelerationStructureBuildSizesKHR         pfn_vkGetAccelerationStructureBuildSizesKHR         = nullptr;
inline PFN_vkGetAccelerationStructureDeviceAddressKHR      pfn_vkGetAccelerationStructureDeviceAddressKHR      = nullptr;
inline PFN_vkCmdBuildAccelerationStructuresKHR             pfn_vkCmdBuildAccelerationStructuresKHR             = nullptr;
inline PFN_vkCmdWriteAccelerationStructuresPropertiesKHR   pfn_vkCmdWriteAccelerationStructuresPropertiesKHR   = nullptr;
inline PFN_vkCmdCopyAccelerationStructureKHR               pfn_vkCmdCopyAccelerationStructureKHR               = nullptr;
inline PFN_vkCreateRayTracingPipelinesKHR                  pfn_vkCreateRayTracingPipelinesKHR                  = nullptr;
inline PFN_vkGetRayTracingShaderGroupHandlesKHR            pfn_vkGetRayTracingShaderGroupHandlesKHR            = nullptr;
inline PFN_vkCmdTraceRaysKHR                               pfn_vkCmdTraceRaysKHR                               = nullptr;

// Push constant layout: 4 buffer device addresses (one per CBV slot).
// Shaders read these via vk::RawBufferLoad<T>(address).
//   slot 0 (b0): 8 bytes — FrameCBuffer BDA
//   slot 1 (b1): 8 bytes — root constants BDA (allocated per-draw from dynamic allocator)
//   slot 2 (b2): 8 bytes — pass CB BDA
//   slot 3 (b3): 8 bytes — draw CB BDA
static constexpr uint32_t VK_PUSH_CONSTANT_SIZE   = 32;
static constexpr uint32_t VK_PUSH_CBV_B0_OFFSET   =  0;
static constexpr uint32_t VK_PUSH_CBV_B1_OFFSET   =  8;
static constexpr uint32_t VK_PUSH_CBV_B2_OFFSET   = 16;
static constexpr uint32_t VK_PUSH_CBV_B3_OFFSET   = 24;
static constexpr uint32_t VK_ROOT_CONSTANT_SIZE    = 32; // 8 x uint32, matches D3D12 root constants at slot 1

// Bindless descriptor set bindings (set 0)
static constexpr uint32_t VK_BINDLESS_SET          = 0;
static constexpr uint32_t VK_BINDLESS_BINDING_SRV  = 0;  // Texture2D / TextureCube / etc.
static constexpr uint32_t VK_BINDLESS_BINDING_UAV  = 1;  // RWTexture2D / etc.
static constexpr uint32_t VK_BINDLESS_BINDING_BUF  = 2;  // ByteAddressBuffer / StructuredBuffer
static constexpr uint32_t VK_BINDLESS_BINDING_RBUF = 3;  // RWByteAddressBuffer / RWStructuredBuffer
static constexpr uint32_t VK_BINDLESS_BINDING_AS   = 4;  // RaytracingAccelerationStructure (only bound when RT is supported)
static constexpr uint32_t VK_BINDLESS_BINDING_COUNT = 5;
static constexpr uint32_t VK_BINDLESS_MAX_COUNT    = 65536;
static constexpr uint32_t VK_BINDLESS_MAX_AS_COUNT = 256;

static constexpr uint32_t VK_SAMPLER_SET = 1;
