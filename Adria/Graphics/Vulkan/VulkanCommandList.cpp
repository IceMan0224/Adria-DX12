#include "VulkanCommandList.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanFence.h"
#include "VulkanQueryHeap.h"
#include "VulkanPipelineState.h"
#include "VulkanCommandQueue.h"
#include "VulkanConversions.h"
#include "VulkanRayTracingAS.h"
#include "VulkanRayTracingPipeline.h"
#include "VulkanRayTracingShaderBindings.h"
#include "Graphics/GfxBufferView.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxProfiler.h"

namespace adria
{
	VulkanCommandList::VulkanCommandList(GfxDevice* in_gfx, GfxCommandListType in_type, Char const* name)
		: gfx(static_cast<VulkanDevice*>(in_gfx)), type(in_type)
	{
		VkDevice device = gfx->GetVkDevice();
		Uint32 family = gfx->GetQueueFamilyIndex(type);

		VkCommandPoolCreateInfo pool_ci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		pool_ci.queueFamilyIndex = family;
		pool_ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK(vkCreateCommandPool(device, &pool_ci, nullptr, &cmd_pool));

		VkCommandBufferAllocateInfo alloc_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool        = cmd_pool;
		alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cmd_buffer));
		if (name)
		{
			VK_OBJECT_SET_NAME(device, cmd_buffer, VK_OBJECT_TYPE_COMMAND_BUFFER, name);
		}
	}

	VulkanCommandList::~VulkanCommandList()
	{
		VkDevice device = gfx->GetVkDevice();
		if (cmd_pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(device, cmd_pool, nullptr);
		}
	}

	GfxCommandQueue* VulkanCommandList::GetQueue() const
	{
		return gfx->GetCommandQueue(type);
	}

	void VulkanCommandList::ResetAllocator()
	{
		VK_CHECK(vkResetCommandPool(gfx->GetVkDevice(), cmd_pool, 0));
	}

	void VulkanCommandList::Begin()
	{
		vkResetCommandBuffer(cmd_buffer, 0);
		VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &begin_info));

		VkDescriptorSet sets[2] = { gfx->GetBindlessDescriptorSet(), gfx->GetSamplerDescriptorSet() };
		VkPipelineLayout layout = gfx->GetCommonPipelineLayout();

		vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			layout, 0, 2, sets, 0, nullptr);
		vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			layout, 0, 2, sets, 0, nullptr);
		if (gfx->IsRayTracingSupported())
		{
			vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
				layout, 0, 2, sets, 0, nullptr);
		}
		memset(push_constants, 0, sizeof(push_constants));
	}

	void VulkanCommandList::End()
	{
		FlushBarriers();
		VK_CHECK(vkEndCommandBuffer(cmd_buffer));
	}

	void VulkanCommandList::Wait(GfxFence& fence, Uint64 value)
	{
		VulkanFence& vk_fence = static_cast<VulkanFence&>(fence);
		pending_waits.emplace_back(vk_fence.GetSemaphore(), value);
	}

	void VulkanCommandList::Signal(GfxFence& fence, Uint64 value)
	{
		VulkanFence& vk_fence = static_cast<VulkanFence&>(fence);
		pending_signals.emplace_back(vk_fence.GetSemaphore(), value);
	}

	void VulkanCommandList::WaitAll()
	{
		if (pending_waits.empty()) 
		{
			return;
		}
		
		std::vector<VkSemaphoreSubmitInfo> wait_infos;
		wait_infos.reserve(pending_waits.size());
		for (auto const& [sem, val] : pending_waits)
		{
			VkSemaphoreSubmitInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
			info.semaphore = sem;
			info.value     = val;
			info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			wait_infos.push_back(info);
		}

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.waitSemaphoreInfoCount = (Uint32)wait_infos.size();
		submit.pWaitSemaphoreInfos    = wait_infos.data();

		VkQueue queue = static_cast<VulkanCommandQueue*>(GetQueue())->GetQueue();
		VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));

		pending_waits.clear();
	}

	void VulkanCommandList::Submit()
	{
		std::vector<VkSemaphoreSubmitInfo> wait_infos, signal_infos;
		for (auto const& [sem, val] : pending_waits)
		{
			VkSemaphoreSubmitInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
			info.semaphore = sem;
			info.value     = val;
			info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			wait_infos.push_back(info);
		}
		for (auto const& [sem, val] : pending_signals)
		{
			VkSemaphoreSubmitInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
			info.semaphore = sem;
			info.value     = val;
			info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			signal_infos.push_back(info);
		}

		VkCommandBufferSubmitInfo cmd_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
		cmd_info.commandBuffer = cmd_buffer;

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.waitSemaphoreInfoCount   = (Uint32)wait_infos.size();
		submit.pWaitSemaphoreInfos      = wait_infos.data();
		submit.commandBufferInfoCount   = 1;
		submit.pCommandBufferInfos      = &cmd_info;
		submit.signalSemaphoreInfoCount = (Uint32)signal_infos.size();
		submit.pSignalSemaphoreInfos    = signal_infos.data();

		VkQueue queue = static_cast<VulkanCommandQueue*>(GetQueue())->GetQueue();
		VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));

		pending_waits.clear();
		pending_signals.clear();
	}

	void VulkanCommandList::SignalAll()
	{
		if (pending_signals.empty())
		{
			return;
		}

		std::vector<VkSemaphoreSubmitInfo> signal_infos;
		signal_infos.reserve(pending_signals.size());
		for (auto const& [sem, val] : pending_signals)
		{
			VkSemaphoreSubmitInfo info{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
			info.semaphore = sem;
			info.value     = val;
			info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			signal_infos.push_back(info);
		}

		VkSubmitInfo2 submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
		submit.signalSemaphoreInfoCount = (Uint32)signal_infos.size();
		submit.pSignalSemaphoreInfos    = signal_infos.data();

		VkQueue queue = static_cast<VulkanCommandQueue*>(GetQueue())->GetQueue();
		VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));

		pending_signals.clear();
	}

	void VulkanCommandList::ResetState()
	{
		pending_waits.clear();
		pending_signals.clear();
		pending_image_barriers.clear();
		pending_buffer_barriers.clear();
		pending_global_barriers.clear();
		memset(push_constants, 0, sizeof(push_constants));
	}

	void VulkanCommandList::BeginEvent(Char const* event_name)
	{
		VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		label.pLabelName = event_name;
		label.color[0]   = 1.0f; label.color[1] = 1.0f; label.color[2] = 1.0f; label.color[3] = 1.0f;
		if (pfn_vkCmdBeginDebugUtilsLabelEXT) 
		{
			pfn_vkCmdBeginDebugUtilsLabelEXT(cmd_buffer, &label);
		}
		g_GfxProfiler.BeginProfileScope(this, event_name);
	}

	void VulkanCommandList::BeginEvent(Char const* event_name, Uint32 event_color)
	{
		VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		label.pLabelName = event_name;
		label.color[0]   = ((event_color >> 16) & 0xFF) / 255.0f;
		label.color[1]   = ((event_color >>  8) & 0xFF) / 255.0f;
		label.color[2]   = ((event_color >>  0) & 0xFF) / 255.0f;
		label.color[3]   = 1.0f;
		if (pfn_vkCmdBeginDebugUtilsLabelEXT) 
		{
			pfn_vkCmdBeginDebugUtilsLabelEXT(cmd_buffer, &label);
		}
		g_GfxProfiler.BeginProfileScope(this, event_name);
	}

	void VulkanCommandList::EndEvent()
	{
		g_GfxProfiler.EndProfileScope(this);
		if (pfn_vkCmdEndDebugUtilsLabelEXT) 
		{
			pfn_vkCmdEndDebugUtilsLabelEXT(cmd_buffer);
		}
	}

	void VulkanCommandList::BeginQuery(GfxQueryHeap& query_heap, Uint32 index)
	{
		VulkanQueryHeap& vk_heap = static_cast<VulkanQueryHeap&>(query_heap);
		if (query_heap.GetDesc().type == GfxQueryType::Timestamp)
		{
			vkCmdWriteTimestamp2(cmd_buffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, vk_heap.GetPool(), index);
		}
		else
		{
			vkCmdBeginQuery(cmd_buffer, vk_heap.GetPool(), index, 0);
		}
	}

	void VulkanCommandList::EndQuery(GfxQueryHeap& query_heap, Uint32 index)
	{
		VulkanQueryHeap& vk_heap = static_cast<VulkanQueryHeap&>(query_heap);
		if (query_heap.GetDesc().type == GfxQueryType::Timestamp)
		{
			vkCmdWriteTimestamp2(cmd_buffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, vk_heap.GetPool(), index);
		}
		else
		{
			vkCmdEndQuery(cmd_buffer, vk_heap.GetPool(), index);
		}
	}

	void VulkanCommandList::ResolveQueryData(GfxQueryHeap const& query_heap, Uint32 start, Uint32 count, GfxBuffer& dst_buffer, Uint64 dst_offset)
	{
		VulkanQueryHeap const& vk_heap = static_cast<VulkanQueryHeap const&>(query_heap);
		VulkanBuffer& vk_buf = static_cast<VulkanBuffer&>(dst_buffer);
		vkCmdCopyQueryPoolResults(cmd_buffer, vk_heap.GetPool(), start, count,
			vk_buf.GetBuffer(), dst_offset, sizeof(Uint64),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	}

	void VulkanCommandList::Draw(Uint32 vertex_count, Uint32 instance_count, Uint32 start_vertex_location, Uint32 start_instance_location)
	{
		vkCmdDraw(cmd_buffer, vertex_count, instance_count, start_vertex_location, start_instance_location);
	}

	void VulkanCommandList::DrawIndexed(Uint32 index_count, Uint32 instance_count, Uint32 index_offset, Uint32 base_vertex_location, Uint32 start_instance_location)
	{
		vkCmdDrawIndexed(cmd_buffer, index_count, instance_count, index_offset, base_vertex_location, start_instance_location);
	}

	void VulkanCommandList::Dispatch(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z)
	{
		vkCmdDispatch(cmd_buffer, group_count_x, group_count_y, group_count_z);
	}

	void VulkanCommandList::DispatchMesh(Uint32 group_count_x, Uint32 group_count_y, Uint32 group_count_z)
	{
		pfn_vkCmdDrawMeshTasksEXT(cmd_buffer, group_count_x, group_count_y, group_count_z);
	}

	void VulkanCommandList::DrawIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		VulkanBuffer const& vk_buf = static_cast<VulkanBuffer const&>(buffer);
		vkCmdDrawIndirect(cmd_buffer, vk_buf.GetBuffer(), offset, 1, sizeof(GfxDrawArguments));
	}

	void VulkanCommandList::DrawIndexedIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		VulkanBuffer const& vk_buf = static_cast<VulkanBuffer const&>(buffer);
		vkCmdDrawIndexedIndirect(cmd_buffer, vk_buf.GetBuffer(), offset, 1, sizeof(GfxDrawIndexedArguments));
	}

	void VulkanCommandList::DispatchIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		VulkanBuffer const& vk_buf = static_cast<VulkanBuffer const&>(buffer);
		vkCmdDispatchIndirect(cmd_buffer, vk_buf.GetBuffer(), offset);
	}

	void VulkanCommandList::DispatchMeshIndirect(GfxBuffer const& buffer, Uint32 offset)
	{
		VulkanBuffer const& vk_buf = static_cast<VulkanBuffer const&>(buffer);
		pfn_vkCmdDrawMeshTasksIndirectEXT(cmd_buffer, vk_buf.GetBuffer(), offset, 1, sizeof(GfxDispatchMeshArguments));
	}

	void VulkanCommandList::DispatchRays(Uint32 dispatch_width, Uint32 dispatch_height, Uint32 dispatch_depth)
	{
		ADRIA_ASSERT(pfn_vkCmdTraceRaysKHR != nullptr && "DispatchRays called but RT extensions are not loaded");
		if (dispatch_width == 0 || dispatch_height == 0 || dispatch_depth == 0)
		{
			return;
		}
		ADRIA_ASSERT(current_rt_bindings != nullptr && "DispatchRays requires a prior BeginRayTracingShaderBindings + Commit");

		VkStridedDeviceAddressRegionKHR raygen{}, miss{}, hit{}, callable{};
		current_rt_bindings->Build(*gfx->GetDynamicAllocator(), raygen, miss, hit, callable);

		pfn_vkCmdTraceRaysKHR(cmd_buffer, &raygen, &miss, &hit, &callable, dispatch_width, dispatch_height, dispatch_depth);
		current_rt_bindings.reset();
	}

	void VulkanCommandList::BuildRayTracingBLAS(GfxRayTracingBLAS* blas)
	{
		ADRIA_ASSERT(blas != nullptr);
		VulkanRayTracingBLAS* vk_blas = static_cast<VulkanRayTracingBLAS*>(blas);
		VulkanBuffer* scratch = static_cast<VulkanBuffer*>(vk_blas->scratch_buffer.get());

		Uint32 scratch_align = gfx->GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
		if (scratch_align == 0) 
		{ 
			scratch_align = 256; 
		}

		VkDeviceAddress scratch_address = scratch->GetGpuAddress();
		Uint64 mask = Uint64(scratch_align) - 1;
		scratch_address = (scratch_address + mask) & ~mask;

		VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.type                      = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		build_info.flags                     = vk_blas->build_flags;
		build_info.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_info.srcAccelerationStructure  = VK_NULL_HANDLE;
		build_info.dstAccelerationStructure  = vk_blas->as;
		build_info.geometryCount             = (Uint32)vk_blas->geometries_vk.size();
		build_info.pGeometries               = vk_blas->geometries_vk.data();
		build_info.scratchData.deviceAddress = scratch_address;

		VkAccelerationStructureBuildRangeInfoKHR const* range_ptr = vk_blas->build_ranges.data();
		pfn_vkCmdBuildAccelerationStructuresKHR(cmd_buffer, 1, &build_info, &range_ptr);
	}

	void VulkanCommandList::BuildRayTracingTLAS(GfxRayTracingTLAS* tlas)
	{
		ADRIA_ASSERT(tlas != nullptr);
		VulkanRayTracingTLAS* vk_tlas = static_cast<VulkanRayTracingTLAS*>(tlas);
		VulkanBuffer* scratch = static_cast<VulkanBuffer*>(vk_tlas->scratch_buffer.get());

		Uint32 scratch_align = gfx->GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
		if (scratch_align == 0) 
		{ 
			scratch_align = 256; 
		}

		VkDeviceAddress scratch_address = scratch->GetGpuAddress();
		Uint64 mask = Uint64(scratch_align) - 1;
		scratch_address = (scratch_address + mask) & ~mask;

		VkAccelerationStructureGeometryKHR tlas_geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		tlas_geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlas_geom.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		tlas_geom.geometry.instances.arrayOfPointers    = VK_FALSE;
		tlas_geom.geometry.instances.data.deviceAddress = vk_tlas->instance_buffer->GetGpuAddress();

		VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		build_info.flags                     = vk_tlas->build_flags;
		build_info.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_info.srcAccelerationStructure  = VK_NULL_HANDLE;
		build_info.dstAccelerationStructure  = vk_tlas->as;
		build_info.geometryCount             = 1;
		build_info.pGeometries               = &tlas_geom;
		build_info.scratchData.deviceAddress = scratch_address;

		VkAccelerationStructureBuildRangeInfoKHR range{};
		range.primitiveCount = vk_tlas->instance_count;
		VkAccelerationStructureBuildRangeInfoKHR const* range_ptr = &range;
		pfn_vkCmdBuildAccelerationStructuresKHR(cmd_buffer, 1, &build_info, &range_ptr);
	}

	void VulkanCommandList::UpdateRayTracingTLAS(GfxRayTracingTLAS* tlas)
	{
		ADRIA_ASSERT(tlas != nullptr);
		VulkanRayTracingTLAS* vk_tlas = static_cast<VulkanRayTracingTLAS*>(tlas);
		ADRIA_ASSERT((vk_tlas->build_flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR) != 0 &&
			"UpdateRayTracingTLAS requires the TLAS to have been built with GfxRayTracingASFlag_AllowUpdate");
		VulkanBuffer* scratch = static_cast<VulkanBuffer*>(vk_tlas->scratch_buffer.get());

		Uint32 scratch_align = gfx->GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
		if (scratch_align == 0) 
		{ 
			scratch_align = 256; 
		}

		VkDeviceAddress scratch_address = scratch->GetGpuAddress();
		Uint64 mask = Uint64(scratch_align) - 1;
		scratch_address = (scratch_address + mask) & ~mask;

		VkAccelerationStructureGeometryKHR tlas_geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		tlas_geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		tlas_geom.geometry.instances = VkAccelerationStructureGeometryInstancesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		tlas_geom.geometry.instances.arrayOfPointers    = VK_FALSE;
		tlas_geom.geometry.instances.data.deviceAddress = vk_tlas->instance_buffer->GetGpuAddress();

		VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		build_info.type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		build_info.flags                     = vk_tlas->build_flags;
		build_info.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
		build_info.srcAccelerationStructure  = vk_tlas->as;
		build_info.dstAccelerationStructure  = vk_tlas->as;
		build_info.geometryCount             = 1;
		build_info.pGeometries               = &tlas_geom;
		build_info.scratchData.deviceAddress = scratch_address;

		VkAccelerationStructureBuildRangeInfoKHR range{};
		range.primitiveCount = vk_tlas->instance_count;
		VkAccelerationStructureBuildRangeInfoKHR const* range_ptr = &range;
		pfn_vkCmdBuildAccelerationStructuresKHR(cmd_buffer, 1, &build_info, &range_ptr);
	}

	void VulkanCommandList::TextureBarrier(GfxTexture const& texture, GfxResourceState flags_before, GfxResourceState flags_after, Uint32 subresource)
	{
		VulkanTexture const& vk_tex = static_cast<VulkanTexture const&>(texture);
		VkImageLayout new_layout = ConvertResourceStateToLayout(flags_after);
		if (new_layout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			return;
		}

		VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		barrier.srcStageMask        = ConvertResourceStateToStage(flags_before);
		barrier.srcAccessMask       = ConvertResourceStateToAccess(flags_before);
		barrier.dstStageMask        = ConvertResourceStateToStage(flags_after);
		barrier.dstAccessMask       = ConvertResourceStateToAccess(flags_after);
		barrier.oldLayout           = ConvertResourceStateToLayout(flags_before);
		barrier.newLayout           = new_layout;
		if (vk_tex.IsLayoutUndefined())
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.srcAccessMask = VK_ACCESS_2_NONE;
		}
		vk_tex.MarkLayoutDefined();
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image               = vk_tex.GetImage();

		VkImageAspectFlags aspect = GetAspectFlags(texture.GetFormat());
		if (subresource == static_cast<Uint32>(-1))
		{
			barrier.subresourceRange = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		}
		else
		{
			Uint32 mip   = subresource % texture.GetDesc().mip_levels;
			Uint32 slice = subresource / texture.GetDesc().mip_levels;
			barrier.subresourceRange = { aspect, mip, 1, slice, 1 };
		}

		pending_image_barriers.push_back(barrier);
	}

	void VulkanCommandList::BufferBarrier(GfxBuffer const& buffer, GfxResourceState flags_before, GfxResourceState flags_after)
	{
		VulkanBuffer const& vk_buf = static_cast<VulkanBuffer const&>(buffer);
		VkBufferMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
		barrier.srcStageMask        = ConvertResourceStateToStage(flags_before);
		barrier.srcAccessMask       = ConvertResourceStateToAccess(flags_before);
		barrier.dstStageMask        = ConvertResourceStateToStage(flags_after);
		barrier.dstAccessMask       = ConvertResourceStateToAccess(flags_after);
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer              = vk_buf.GetBuffer();
		barrier.offset              = 0;
		barrier.size                = VK_WHOLE_SIZE;
		pending_buffer_barriers.push_back(barrier);
	}

	void VulkanCommandList::GlobalBarrier(GfxResourceState flags_before, GfxResourceState flags_after)
	{
		VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		barrier.srcStageMask  = ConvertResourceStateToStage(flags_before);
		barrier.srcAccessMask = ConvertResourceStateToAccess(flags_before);
		barrier.dstStageMask  = ConvertResourceStateToStage(flags_after);
		barrier.dstAccessMask = ConvertResourceStateToAccess(flags_after);
		pending_global_barriers.push_back(barrier);
	}

	void VulkanCommandList::FlushBarriers()
	{
		if (pending_image_barriers.empty() && pending_buffer_barriers.empty() && pending_global_barriers.empty())
		{
			return;
		}

		VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dep.memoryBarrierCount       = (Uint32)pending_global_barriers.size();
		dep.pMemoryBarriers          = pending_global_barriers.data();
		dep.bufferMemoryBarrierCount = (Uint32)pending_buffer_barriers.size();
		dep.pBufferMemoryBarriers    = pending_buffer_barriers.data();
		dep.imageMemoryBarrierCount  = (Uint32)pending_image_barriers.size();
		dep.pImageMemoryBarriers     = pending_image_barriers.data();

		vkCmdPipelineBarrier2(cmd_buffer, &dep);

		pending_global_barriers.clear();
		pending_buffer_barriers.clear();
		pending_image_barriers.clear();
	}

	void VulkanCommandList::CopyBuffer(GfxBuffer& dst, GfxBuffer const& src)
	{
		VulkanBuffer& vk_dst = static_cast<VulkanBuffer&>(dst);
		VulkanBuffer const& vk_src = static_cast<VulkanBuffer const&>(src);
		VkBufferCopy region{};
		region.size = std::min(dst.GetSize(), src.GetSize());
		vkCmdCopyBuffer(cmd_buffer, vk_src.GetBuffer(), vk_dst.GetBuffer(), 1, &region);
	}

	void VulkanCommandList::CopyBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxBuffer const& src, Uint64 src_offset, Uint64 size)
	{
		VulkanBuffer& vk_dst = static_cast<VulkanBuffer&>(dst);
		VulkanBuffer const& vk_src = static_cast<VulkanBuffer const&>(src);
		VkBufferCopy region{};
		region.dstOffset = dst_offset;
		region.srcOffset = src_offset;
		region.size      = size;
		vkCmdCopyBuffer(cmd_buffer, vk_src.GetBuffer(), vk_dst.GetBuffer(), 1, &region);
	}

	void VulkanCommandList::CopyTexture(GfxTexture& dst, GfxTexture const& src)
	{
		CopyTexture(dst, 0, 0, src, 0, 0);
	}

	void VulkanCommandList::CopyTexture(GfxTexture& dst, Uint32 dst_mip, Uint32 dst_array, GfxTexture const& src, Uint32 src_mip, Uint32 src_array)
	{
		VulkanTexture& vk_dst = static_cast<VulkanTexture&>(dst);
		VulkanTexture const& vk_src = static_cast<VulkanTexture const&>(src);
		VkImageAspectFlags aspect = GetAspectFlags(src.GetFormat());

		VkImageCopy region{};
		region.srcSubresource = { aspect, src_mip, src_array, 1 };
		region.dstSubresource = { aspect, dst_mip, dst_array, 1 };
		region.extent         = { std::max(1u, src.GetWidth() >> src_mip),
		                          std::max(1u, src.GetHeight() >> src_mip), 1u };

		vkCmdCopyImage(cmd_buffer,
			vk_src.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk_dst.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region);
	}

	void VulkanCommandList::CopyTextureToBuffer(GfxBuffer& dst, Uint64 dst_offset, GfxTexture const& src, Uint32 src_mip, Uint32 src_array)
	{
		VulkanBuffer& vk_dst = static_cast<VulkanBuffer&>(dst);
		VulkanTexture const& vk_src = static_cast<VulkanTexture const&>(src);

		VkBufferImageCopy region{};
		region.bufferOffset      = dst_offset;
		region.imageSubresource  = { GetAspectFlags(src.GetFormat()), src_mip, src_array, 1 };
		region.imageExtent       = { std::max(1u, src.GetWidth() >> src_mip),
		                             std::max(1u, src.GetHeight() >> src_mip), 1u };

		vkCmdCopyImageToBuffer(cmd_buffer,
			vk_src.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk_dst.GetBuffer(), 1, &region);
	}

	void VulkanCommandList::CopyBufferToTexture(GfxTexture& dst_texture, Uint32 mip_level, Uint32 array_slice, GfxBuffer const& src_buffer, Uint32 offset)
	{
		VulkanTexture& vk_dst = static_cast<VulkanTexture&>(dst_texture);
		VulkanBuffer const& vk_src = static_cast<VulkanBuffer const&>(src_buffer);

		Uint32 mip_depth = (dst_texture.GetDesc().type == GfxTextureType_3D)
			? std::max(1u, dst_texture.GetDepth() >> mip_level)
			: 1u;

		VkBufferImageCopy region{};
		region.bufferOffset     = offset;
		region.imageSubresource = { GetAspectFlags(dst_texture.GetFormat()), mip_level, array_slice, 1 };
		region.imageExtent      = { std::max(1u, dst_texture.GetWidth() >> mip_level),
		                            std::max(1u, dst_texture.GetHeight() >> mip_level), mip_depth };

		vkCmdCopyBufferToImage(cmd_buffer,
			vk_src.GetBuffer(),
			vk_dst.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region);
	}

	namespace
	{
		inline Uint16 FloatToHalf(Float value)
		{
			Uint32 bits;
			std::memcpy(&bits, &value, sizeof(bits));
			Uint32 sign = (bits >> 16) & 0x8000u;
			Int32 exp  = ((bits >> 23) & 0xff) - 127 + 15;
			Uint32 mant = bits & 0x7fffffu;
			if (exp >= 31) return (Uint16)(sign | 0x7c00u); // Inf/NaN/overflow -> Inf
			if (exp <= 0)  return (Uint16)sign;             // underflow -> signed zero
			return (Uint16)(sign | (exp << 10) | (mant >> 13));
		}

		Uint32 PackClearFloatForBuffer(GfxFormat format, Float const clear[4])
		{
			union { Uint32 u32; Float f32; Uint16 h16[2]; Uint8 b8[4]; };
			u32 = 0;
			switch (format)
			{
			case GfxFormat::R32_FLOAT:
				f32 = clear[0];
				return u32;
			case GfxFormat::R32_UINT:
			case GfxFormat::R32_SINT:
				std::memcpy(&u32, &clear[0], sizeof(u32));
				return u32;
			case GfxFormat::R16G16_FLOAT:
				h16[0] = FloatToHalf(clear[0]);
				h16[1] = FloatToHalf(clear[1]);
				return u32;
			case GfxFormat::R16_FLOAT:
				h16[0] = FloatToHalf(clear[0]);
				h16[1] = FloatToHalf(clear[0]); 
				return u32;
			case GfxFormat::R8G8B8A8_UNORM:
			case GfxFormat::R8G8B8A8_UNORM_SRGB:
				b8[0] = (Uint8)std::clamp(clear[0] * 255.0f, 0.0f, 255.0f);
				b8[1] = (Uint8)std::clamp(clear[1] * 255.0f, 0.0f, 255.0f);
				b8[2] = (Uint8)std::clamp(clear[2] * 255.0f, 0.0f, 255.0f);
				b8[3] = (Uint8)std::clamp(clear[3] * 255.0f, 0.0f, 255.0f);
				return u32;
			case GfxFormat::B8G8R8A8_UNORM:
			case GfxFormat::B8G8R8A8_UNORM_SRGB:
				b8[0] = (Uint8)std::clamp(clear[2] * 255.0f, 0.0f, 255.0f);
				b8[1] = (Uint8)std::clamp(clear[1] * 255.0f, 0.0f, 255.0f);
				b8[2] = (Uint8)std::clamp(clear[0] * 255.0f, 0.0f, 255.0f);
				b8[3] = (Uint8)std::clamp(clear[3] * 255.0f, 0.0f, 255.0f);
				return u32;
			case GfxFormat::UNKNOWN:
			default:
				std::memcpy(&u32, &clear[0], sizeof(u32));
				return u32;
			}
		}
	}

	void VulkanCommandList::ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Float const clear_value[4])
	{
		FlushBarriers();
		VulkanBuffer const& vk_buf = static_cast<VulkanBuffer const&>(resource);
		Uint64 size = (uav_desc.size == Uint64(-1)) ? (resource.GetDesc().size - uav_desc.offset) : uav_desc.size;
		Uint32 packed = PackClearFloatForBuffer(resource.GetDesc().format, clear_value);
		vkCmdFillBuffer(cmd_buffer, vk_buf.GetBuffer(), uav_desc.offset, size, packed);
	}

	void VulkanCommandList::ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Float const clear_value[4])
	{
		FlushBarriers();
		VulkanTexture const& vk_tex = static_cast<VulkanTexture const&>(resource);
		VkClearColorValue color{};
		color.float32[0] = clear_value[0]; color.float32[1] = clear_value[1];
		color.float32[2] = clear_value[2]; color.float32[3] = clear_value[3];
		Uint32 const mip_count   = (uav_desc.mip_count   == Uint32(-1)) ? resource.GetDesc().mip_levels : uav_desc.mip_count;
		Uint32 const slice_count = (uav_desc.slice_count == Uint32(-1)) ? std::max(1u, resource.GetDesc().array_size - uav_desc.first_slice) : uav_desc.slice_count;
		VkImageSubresourceRange range{ GetAspectFlags(resource.GetFormat()), uav_desc.first_mip, mip_count, uav_desc.first_slice, slice_count };
		vkCmdClearColorImage(cmd_buffer, vk_tex.GetImage(), VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);
	}

	void VulkanCommandList::ClearBuffer(GfxBuffer const& resource, GfxBufferDescriptorDesc const& uav_desc, Uint32 const clear_value[4])
	{
		FlushBarriers();
		VulkanBuffer const& vk_buf = static_cast<VulkanBuffer const&>(resource);
		Uint64 size = (uav_desc.size == Uint64(-1)) ? (resource.GetDesc().size - uav_desc.offset) : uav_desc.size;
		vkCmdFillBuffer(cmd_buffer, vk_buf.GetBuffer(), uav_desc.offset, size, clear_value[0]);
	}

	void VulkanCommandList::ClearTexture(GfxTexture const& resource, GfxTextureDescriptorDesc const& uav_desc, Uint32 const clear_value[4])
	{
		FlushBarriers();
		VulkanTexture const& vk_tex = static_cast<VulkanTexture const&>(resource);
		VkClearColorValue color{};
		color.uint32[0] = clear_value[0]; color.uint32[1] = clear_value[1];
		color.uint32[2] = clear_value[2]; color.uint32[3] = clear_value[3];
		Uint32 const mip_count   = (uav_desc.mip_count   == Uint32(-1)) ? resource.GetDesc().mip_levels : uav_desc.mip_count;
		Uint32 const slice_count = (uav_desc.slice_count == Uint32(-1)) ? std::max(1u, resource.GetDesc().array_size - uav_desc.first_slice) : uav_desc.slice_count;
		VkImageSubresourceRange range{ GetAspectFlags(resource.GetFormat()), uav_desc.first_mip, mip_count, uav_desc.first_slice, slice_count };
		vkCmdClearColorImage(cmd_buffer, vk_tex.GetImage(), VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);
	}

	void VulkanCommandList::WriteBufferImmediate(GfxBuffer& buffer, Uint32 offset, Uint32 data)
	{
		VulkanBuffer& vk_buf = static_cast<VulkanBuffer&>(buffer);
		vkCmdFillBuffer(cmd_buffer, vk_buf.GetBuffer(), offset, sizeof(Uint32), data);
	}

	void VulkanCommandList::BeginRenderPass(GfxRenderPassDesc const& desc)
	{
		FlushBarriers();

		std::vector<VkRenderingAttachmentInfo> color_attachments;
		color_attachments.reserve(desc.rtv_attachments.size());
		for (auto const& rtv : desc.rtv_attachments)
		{
			VkImageView view = (VkImageView)rtv.cpu_handle.opaque_data[1];

			VkRenderingAttachmentInfo att{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
			att.imageView   = view;
			att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			att.loadOp      = ConvertLoadOp(rtv.beginning_access);
			att.storeOp     = ConvertStoreOp(rtv.ending_access);

			if (rtv.beginning_access == GfxLoadAccessOp::Clear)
			{
				att.clearValue.color.float32[0] = rtv.clear_value.color.color[0];
				att.clearValue.color.float32[1] = rtv.clear_value.color.color[1];
				att.clearValue.color.float32[2] = rtv.clear_value.color.color[2];
				att.clearValue.color.float32[3] = rtv.clear_value.color.color[3];
			}
			color_attachments.push_back(att);
		}

		VkRenderingAttachmentInfo depth_att{};
		VkRenderingAttachmentInfo stencil_att{};
		Bool has_depth = desc.dsv_attachment.has_value();
		Bool has_stencil = false;
		if (has_depth)
		{
			auto const& dsv = *desc.dsv_attachment;
			VkImageView view = (VkImageView)dsv.cpu_handle.opaque_data[1];

			depth_att.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depth_att.imageView   = view;
			depth_att.imageLayout = (desc.flags & GfxRenderPassFlagBit_ReadOnlyDepth)
				? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depth_att.loadOp      = ConvertLoadOp(dsv.depth_beginning_access);
			depth_att.storeOp     = ConvertStoreOp(dsv.depth_ending_access);

			if (dsv.depth_beginning_access == GfxLoadAccessOp::Clear)
			{
				depth_att.clearValue.depthStencil.depth   = dsv.clear_value.depth_stencil.depth;
				depth_att.clearValue.depthStencil.stencil = dsv.clear_value.depth_stencil.stencil;
			}

			has_stencil = dsv.stencil_beginning_access != GfxLoadAccessOp::NoAccess;
			if (has_stencil)
			{
				stencil_att.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				stencil_att.imageView   = view;
				stencil_att.imageLayout = (desc.flags & GfxRenderPassFlagBit_ReadOnlyStencil)
					? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
					: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				stencil_att.loadOp      = ConvertLoadOp(dsv.stencil_beginning_access);
				stencil_att.storeOp     = ConvertStoreOp(dsv.stencil_ending_access);

				if (dsv.stencil_beginning_access == GfxLoadAccessOp::Clear)
				{
					stencil_att.clearValue.depthStencil.depth   = dsv.clear_value.depth_stencil.depth;
					stencil_att.clearValue.depthStencil.stencil = dsv.clear_value.depth_stencil.stencil;
				}
			}
		}

		VkRenderingInfo rendering{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		rendering.renderArea           = { { 0, 0 }, { desc.width, desc.height } };
		rendering.layerCount           = 1;
		rendering.colorAttachmentCount = (Uint32)color_attachments.size();
		rendering.pColorAttachments    = color_attachments.data();
		rendering.pDepthAttachment     = has_depth ? &depth_att : nullptr;
		rendering.pStencilAttachment   = has_stencil ? &stencil_att : nullptr;

		vkCmdBeginRendering(cmd_buffer, &rendering);
		SetViewport(0, 0, desc.width, desc.height);
	}

	void VulkanCommandList::EndRenderPass()
	{
		vkCmdEndRendering(cmd_buffer);
	}

	void VulkanCommandList::SetPipelineState(GfxPipelineState const* state)
	{
		if (!state) 
		{ 
			return; 
		}

		VulkanPipelineState const* vk_pso = static_cast<VulkanPipelineState const*>(state);
		VkPipelineBindPoint bind_point =
			state->GetType() == GfxPipelineStateType::Compute
			? VK_PIPELINE_BIND_POINT_COMPUTE
			: VK_PIPELINE_BIND_POINT_GRAPHICS;
		vkCmdBindPipeline(cmd_buffer, bind_point, vk_pso->GetPipeline());
	}

	GfxRayTracingShaderBindings* VulkanCommandList::BeginRayTracingShaderBindings(GfxRayTracingPipeline const* pipeline)
	{
		ADRIA_ASSERT(pipeline != nullptr && pipeline->IsValid());
		VulkanRayTracingPipeline const* vk_pipeline = static_cast<VulkanRayTracingPipeline const*>(pipeline);
		vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk_pipeline->GetVkPipeline());
		current_rt_bindings = std::make_unique<VulkanRayTracingShaderBindings>(vk_pipeline);
		return current_rt_bindings.get();
	}

	void VulkanCommandList::SetStencilReference(Uint8 stencil)
	{
		vkCmdSetStencilReference(cmd_buffer, VK_STENCIL_FACE_FRONT_AND_BACK, stencil);
	}

	void VulkanCommandList::SetBlendFactor(Float const* blend_factor)
	{
		vkCmdSetBlendConstants(cmd_buffer, blend_factor);
	}

	void VulkanCommandList::SetPrimitiveTopology(GfxPrimitiveTopology topology)
	{
		vkCmdSetPrimitiveTopology(cmd_buffer, ConvertTopology(topology));
	}

	void VulkanCommandList::SetIndexBuffer(GfxIndexBufferView* view)
	{
		if (!view) 
		{ 
			return; 
		}

		VkIndexType index_type = view->format == GfxFormat::R16_UINT ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		VulkanBufferLookup lookup = gfx->GetBufferFromAddress(view->buffer_location);
		vkCmdBindIndexBuffer(cmd_buffer, lookup.buffer, lookup.offset, index_type);
	}

	void VulkanCommandList::SetVertexBuffer(GfxVertexBufferView const& view, Uint32 start_slot)
	{
		VulkanBufferLookup lookup = gfx->GetBufferFromAddress(view.buffer_location);
		VkDeviceSize offset = lookup.offset;
		vkCmdBindVertexBuffers(cmd_buffer, start_slot, 1, &lookup.buffer, &offset);
	}

	void VulkanCommandList::SetVertexBuffers(std::span<GfxVertexBufferView const> views, Uint32 start_slot)
	{
		for (Uint32 i = 0; i < (Uint32)views.size(); ++i)
		{
			SetVertexBuffer(views[i], start_slot + i);
		}
	}

	void VulkanCommandList::SetViewport(Uint32 x, Uint32 y, Uint32 width, Uint32 height)
	{
		VkViewport viewport{};
		viewport.x        = (Float)x;
		viewport.y        = (Float)(y + height); 
		viewport.width    = (Float)width;
		viewport.height   = -(Float)height;      
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
		SetScissorRect(x, y, width, height);
	}

	void VulkanCommandList::SetScissorRect(Uint32 x, Uint32 y, Uint32 width, Uint32 height)
	{
		VkRect2D scissor{ { (Int32)x, (Int32)y }, { width, height } };
		vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);
	}

	void VulkanCommandList::SetShadingRate(GfxShadingRate, std::span<GfxShadingRateCombiner, SHADING_RATE_COMBINER_COUNT>) {}
	void VulkanCommandList::SetShadingRate(GfxShadingRate) {}
	void VulkanCommandList::SetShadingRateImage(GfxTexture const*) {}
	void VulkanCommandList::BeginVRS(GfxShadingRateInfo const&) {}
	void VulkanCommandList::EndVRS(GfxShadingRateInfo const&) {}

	void VulkanCommandList::SetRootConstant(Uint32 slot, Uint32 data, Uint32 offset)
	{
		memcpy(root_constant_shadow + offset * sizeof(Uint32), &data, sizeof(Uint32));
		GfxDynamicAllocation alloc = AllocateTransient(VK_ROOT_CONSTANT_SIZE, GFX_CONSTANT_BUFFER_DATA_ALIGNMENT);
		memcpy(alloc.cpu_address, root_constant_shadow, VK_ROOT_CONSTANT_SIZE);
		SetRootCBV(1, alloc.gpu_address);
	}

	void VulkanCommandList::SetRootConstants(Uint32 slot, void const* data, Uint32 data_size, Uint32 offset)
	{
		memcpy(root_constant_shadow + offset * sizeof(Uint32), data, data_size);
		GfxDynamicAllocation alloc = AllocateTransient(VK_ROOT_CONSTANT_SIZE, GFX_CONSTANT_BUFFER_DATA_ALIGNMENT);
		memcpy(alloc.cpu_address, root_constant_shadow, VK_ROOT_CONSTANT_SIZE);
		SetRootCBV(1, alloc.gpu_address);
	}

	void VulkanCommandList::SetRootCBV(Uint32 slot, Uint64 gpu_address)
	{
		Uint32 byte_offset = 0;
		switch (slot)
		{
		case 0: byte_offset = VK_PUSH_CBV_B0_OFFSET; break;
		case 1: byte_offset = VK_PUSH_CBV_B1_OFFSET; break;
		case 2: byte_offset = VK_PUSH_CBV_B2_OFFSET; break;
		case 3: byte_offset = VK_PUSH_CBV_B3_OFFSET; break;
		default: ADRIA_ASSERT(false && "Invalid CBV slot"); return;
		}
		memcpy(push_constants + byte_offset, &gpu_address, sizeof(Uint64));
		vkCmdPushConstants(cmd_buffer, gfx->GetCommonPipelineLayout(),
			VK_SHADER_STAGE_ALL, byte_offset, sizeof(Uint64), &gpu_address);
	}

	void VulkanCommandList::SetRootCBV(Uint32 slot, void const* data, Uint64 data_size)
	{
		GfxDynamicAllocation alloc = AllocateTransient((Uint32)data_size, GFX_CONSTANT_BUFFER_DATA_ALIGNMENT);
		alloc.Update(data, data_size);
		SetRootCBV(slot, alloc.gpu_address);
	}

	void VulkanCommandList::SetRootSRV(Uint32 slot, Uint64 gpu_address)
	{
		ADRIA_ASSERT(false && "SetRootSRV not implemented for Vulkan common root signature");
	}

	void VulkanCommandList::SetRootUAV(Uint32 slot, Uint64 gpu_address)
	{
		ADRIA_ASSERT(false && "SetRootUAV not implemented for Vulkan common root signature");
	}

	void VulkanCommandList::SetRootDescriptorTable(Uint32 slot, GfxDescriptor base_descriptor)
	{
		
	}

	GfxDynamicAllocation VulkanCommandList::AllocateTransient(Uint32 size, Uint32 align)
	{
		return gfx->GetDynamicAllocator()->Allocate(size, align);
	}

	void VulkanCommandList::ClearRenderTarget(GfxDescriptor rtv, Float const* clear_color)
	{
		// BeginRenderPass with Clear loadOp is the Vulkan way;
		// Left as no-op; callers should use BeginRenderPass with Clear loadOp
		VkImageView view = (VkImageView)rtv.opaque_data[1];
	}

	void VulkanCommandList::ClearDepth(GfxDescriptor dsv, Float depth, Uint8 stencil, Bool clear_stencil)
	{
	}

	void VulkanCommandList::SetRenderTargets(std::span<GfxDescriptor const> rtvs, GfxDescriptor const* dsv, Bool single_rt)
	{
	}

	void VulkanCommandList::SetContext(Context ctx)
	{
		// No-op in Vulkan; pipeline bind point is determined at SetPipelineState
	}
}
