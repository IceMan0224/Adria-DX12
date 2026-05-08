#pragma once
#include "VulkanDefines.h"
#include "Graphics/GfxFormat.h"
#include "Graphics/GfxResource.h"
#include "Graphics/GfxStates.h"
#include "Graphics/GfxRenderPass.h"
#include "Graphics/GfxQueryHeap.h"

namespace adria
{
	VkFormat             ConvertFormat(GfxFormat format);
	GfxFormat            ConvertFormat(VkFormat format);
	VkSampleCountFlagBits ConvertSampleCount(Uint32 count);

	VkImageLayout        ConvertResourceStateToLayout(GfxResourceState state);
	VkAccessFlags2       ConvertResourceStateToAccess(GfxResourceState state);
	VkPipelineStageFlags2 ConvertResourceStateToStage(GfxResourceState state);

	VkBlendFactor        ConvertBlendFactor(GfxBlend blend);
	VkBlendOp            ConvertBlendOp(GfxBlendOp op);
	VkCompareOp          ConvertCompareOp(GfxComparisonFunc func);
	VkStencilOp          ConvertStencilOp(GfxStencilOp op);
	VkPolygonMode        ConvertFillMode(GfxFillMode mode);
	VkCullModeFlags      ConvertCullMode(GfxCullMode mode);
	VkPrimitiveTopology  ConvertTopology(GfxPrimitiveTopology topology);
	VkAttachmentLoadOp   ConvertLoadOp(GfxLoadAccessOp op);
	VkAttachmentStoreOp  ConvertStoreOp(GfxStoreAccessOp op);
	VkQueryType          ConvertQueryType(GfxQueryType type);
	VkColorComponentFlags ConvertColorWrite(GfxColorWrite write);
	VkImageAspectFlags   GetAspectFlags(GfxFormat format);
	VkImageAspectFlags   GetAspectFlags(VkFormat format);
}
