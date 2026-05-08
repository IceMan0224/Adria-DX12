#include "VulkanConversions.h"

namespace adria
{
	VkFormat ConvertFormat(GfxFormat format)
	{
		switch (format)
		{
		case GfxFormat::UNKNOWN:                return VK_FORMAT_UNDEFINED;
		case GfxFormat::R32G32B32A32_FLOAT:     return VK_FORMAT_R32G32B32A32_SFLOAT;
		case GfxFormat::R32G32B32A32_UINT:      return VK_FORMAT_R32G32B32A32_UINT;
		case GfxFormat::R32G32B32A32_SINT:      return VK_FORMAT_R32G32B32A32_SINT;
		case GfxFormat::R32G32B32_FLOAT:        return VK_FORMAT_R32G32B32_SFLOAT;
		case GfxFormat::R32G32B32_UINT:         return VK_FORMAT_R32G32B32_UINT;
		case GfxFormat::R32G32B32_SINT:         return VK_FORMAT_R32G32B32_SINT;
		case GfxFormat::R16G16B16A16_FLOAT:     return VK_FORMAT_R16G16B16A16_SFLOAT;
		case GfxFormat::R16G16B16A16_UNORM:     return VK_FORMAT_R16G16B16A16_UNORM;
		case GfxFormat::R16G16B16A16_UINT:      return VK_FORMAT_R16G16B16A16_UINT;
		case GfxFormat::R16G16B16A16_SNORM:     return VK_FORMAT_R16G16B16A16_SNORM;
		case GfxFormat::R16G16B16A16_SINT:      return VK_FORMAT_R16G16B16A16_SINT;
		case GfxFormat::R32G32_FLOAT:           return VK_FORMAT_R32G32_SFLOAT;
		case GfxFormat::R32G32_UINT:            return VK_FORMAT_R32G32_UINT;
		case GfxFormat::R32G32_SINT:            return VK_FORMAT_R32G32_SINT;
		case GfxFormat::R32G8X24_TYPELESS:      return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case GfxFormat::D32_FLOAT_S8X24_UINT:   return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case GfxFormat::R10G10B10A2_UNORM:      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case GfxFormat::R10G10B10A2_UINT:       return VK_FORMAT_A2B10G10R10_UINT_PACK32;
		case GfxFormat::R11G11B10_FLOAT:        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case GfxFormat::R8G8B8A8_UNORM:         return VK_FORMAT_R8G8B8A8_UNORM;
		case GfxFormat::R8G8B8A8_UNORM_SRGB:    return VK_FORMAT_R8G8B8A8_SRGB;
		case GfxFormat::R8G8B8A8_UINT:          return VK_FORMAT_R8G8B8A8_UINT;
		case GfxFormat::R8G8B8A8_SNORM:         return VK_FORMAT_R8G8B8A8_SNORM;
		case GfxFormat::R8G8B8A8_SINT:          return VK_FORMAT_R8G8B8A8_SINT;
		case GfxFormat::B8G8R8A8_UNORM:         return VK_FORMAT_B8G8R8A8_UNORM;
		case GfxFormat::B8G8R8A8_UNORM_SRGB:    return VK_FORMAT_B8G8R8A8_SRGB;
		case GfxFormat::R16G16_FLOAT:           return VK_FORMAT_R16G16_SFLOAT;
		case GfxFormat::R16G16_UNORM:           return VK_FORMAT_R16G16_UNORM;
		case GfxFormat::R16G16_UINT:            return VK_FORMAT_R16G16_UINT;
		case GfxFormat::R16G16_SNORM:           return VK_FORMAT_R16G16_SNORM;
		case GfxFormat::R16G16_SINT:            return VK_FORMAT_R16G16_SINT;
		case GfxFormat::R32_TYPELESS:           return VK_FORMAT_D32_SFLOAT;
		case GfxFormat::D32_FLOAT:              return VK_FORMAT_D32_SFLOAT;
		case GfxFormat::R32_FLOAT:              return VK_FORMAT_R32_SFLOAT;
		case GfxFormat::R32_UINT:               return VK_FORMAT_R32_UINT;
		case GfxFormat::R32_SINT:               return VK_FORMAT_R32_SINT;
		case GfxFormat::R24G8_TYPELESS:         return VK_FORMAT_D24_UNORM_S8_UINT;
		case GfxFormat::D24_UNORM_S8_UINT:      return VK_FORMAT_D24_UNORM_S8_UINT;
		case GfxFormat::R8G8_UNORM:             return VK_FORMAT_R8G8_UNORM;
		case GfxFormat::R8G8_UINT:              return VK_FORMAT_R8G8_UINT;
		case GfxFormat::R8G8_SNORM:             return VK_FORMAT_R8G8_SNORM;
		case GfxFormat::R8G8_SINT:              return VK_FORMAT_R8G8_SINT;
		case GfxFormat::R16_TYPELESS:           return VK_FORMAT_D16_UNORM;
		case GfxFormat::R16_FLOAT:              return VK_FORMAT_R16_SFLOAT;
		case GfxFormat::D16_UNORM:              return VK_FORMAT_D16_UNORM;
		case GfxFormat::R16_UNORM:              return VK_FORMAT_R16_UNORM;
		case GfxFormat::R16_UINT:               return VK_FORMAT_R16_UINT;
		case GfxFormat::R16_SNORM:              return VK_FORMAT_R16_SNORM;
		case GfxFormat::R16_SINT:               return VK_FORMAT_R16_SINT;
		case GfxFormat::R8_UNORM:               return VK_FORMAT_R8_UNORM;
		case GfxFormat::R8_UINT:                return VK_FORMAT_R8_UINT;
		case GfxFormat::R8_SNORM:               return VK_FORMAT_R8_SNORM;
		case GfxFormat::R8_SINT:                return VK_FORMAT_R8_SINT;
		case GfxFormat::BC1_UNORM:              return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case GfxFormat::BC1_UNORM_SRGB:         return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		case GfxFormat::BC2_UNORM:              return VK_FORMAT_BC2_UNORM_BLOCK;
		case GfxFormat::BC2_UNORM_SRGB:         return VK_FORMAT_BC2_SRGB_BLOCK;
		case GfxFormat::BC3_UNORM:              return VK_FORMAT_BC3_UNORM_BLOCK;
		case GfxFormat::BC3_UNORM_SRGB:         return VK_FORMAT_BC3_SRGB_BLOCK;
		case GfxFormat::BC4_UNORM:              return VK_FORMAT_BC4_UNORM_BLOCK;
		case GfxFormat::BC4_SNORM:              return VK_FORMAT_BC4_SNORM_BLOCK;
		case GfxFormat::BC5_UNORM:              return VK_FORMAT_BC5_UNORM_BLOCK;
		case GfxFormat::BC5_SNORM:              return VK_FORMAT_BC5_SNORM_BLOCK;
		case GfxFormat::BC6H_UF16:              return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case GfxFormat::BC6H_SF16:              return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case GfxFormat::BC7_UNORM:              return VK_FORMAT_BC7_UNORM_BLOCK;
		case GfxFormat::BC7_UNORM_SRGB:         return VK_FORMAT_BC7_SRGB_BLOCK;
		case GfxFormat::R9G9B9E5_SHAREDEXP:     return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
		default: ADRIA_ASSERT(false && "Unsupported format"); return VK_FORMAT_UNDEFINED;
		}
	}

	GfxFormat ConvertFormat(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_UNDEFINED:                return GfxFormat::UNKNOWN;
		case VK_FORMAT_R8G8B8A8_UNORM:           return GfxFormat::R8G8B8A8_UNORM;
		case VK_FORMAT_R8G8B8A8_SRGB:            return GfxFormat::R8G8B8A8_UNORM_SRGB;
		case VK_FORMAT_B8G8R8A8_UNORM:           return GfxFormat::B8G8R8A8_UNORM;
		case VK_FORMAT_B8G8R8A8_SRGB:            return GfxFormat::B8G8R8A8_UNORM_SRGB;
		case VK_FORMAT_R32_SFLOAT:               return GfxFormat::R32_FLOAT;
		case VK_FORMAT_R32_UINT:                 return GfxFormat::R32_UINT;
		case VK_FORMAT_D32_SFLOAT:               return GfxFormat::D32_FLOAT;
		case VK_FORMAT_D24_UNORM_S8_UINT:        return GfxFormat::D24_UNORM_S8_UINT;
		case VK_FORMAT_R16G16B16A16_SFLOAT:      return GfxFormat::R16G16B16A16_FLOAT;
		case VK_FORMAT_R32G32B32A32_SFLOAT:      return GfxFormat::R32G32B32A32_FLOAT;
		default: ADRIA_ASSERT(false && "Unsupported VkFormat"); return GfxFormat::UNKNOWN;
		}
	}

	VkSampleCountFlagBits ConvertSampleCount(Uint32 count)
	{
		switch (count)
		{
		case 1:  return VK_SAMPLE_COUNT_1_BIT;
		case 2:  return VK_SAMPLE_COUNT_2_BIT;
		case 4:  return VK_SAMPLE_COUNT_4_BIT;
		case 8:  return VK_SAMPLE_COUNT_8_BIT;
		case 16: return VK_SAMPLE_COUNT_16_BIT;
		default: return VK_SAMPLE_COUNT_1_BIT;
		}
	}

	VkImageLayout ConvertResourceStateToLayout(GfxResourceState state)
	{
		if (HasAnyFlag(state, GfxResourceState::RTV))          { return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; }
		if (HasAnyFlag(state, GfxResourceState::DSV))          { return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; }
		if (HasAnyFlag(state, GfxResourceState::DSV_ReadOnly)) { return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; }
		if (HasAnyFlag(state, GfxResourceState::AllSRV))       { return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }
		if (HasAnyFlag(state, GfxResourceState::AllUAV))       { return VK_IMAGE_LAYOUT_GENERAL; }
		if (HasAnyFlag(state, GfxResourceState::CopyDst))      { return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; }
		if (HasAnyFlag(state, GfxResourceState::CopySrc))      { return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; }
		if (HasAnyFlag(state, GfxResourceState::Present))      { return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; }
		if (HasAnyFlag(state, GfxResourceState::Common))       { return VK_IMAGE_LAYOUT_GENERAL; }
		if (state == GfxResourceState::None)                   { return VK_IMAGE_LAYOUT_UNDEFINED; }
		return VK_IMAGE_LAYOUT_GENERAL;
	}

	VkAccessFlags2 ConvertResourceStateToAccess(GfxResourceState state)
	{
		VkAccessFlags2 flags = VK_ACCESS_2_NONE;
		if (HasAnyFlag(state, GfxResourceState::RTV))          flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		if (HasAnyFlag(state, GfxResourceState::DSV))          flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		if (HasAnyFlag(state, GfxResourceState::DSV_ReadOnly)) flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		if (HasAnyFlag(state, GfxResourceState::AllSRV))       flags |= VK_ACCESS_2_SHADER_READ_BIT;
		if (HasAnyFlag(state, GfxResourceState::AllUAV))       flags |= VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
		if (HasAnyFlag(state, GfxResourceState::CopyDst))      flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
		if (HasAnyFlag(state, GfxResourceState::CopySrc))      flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
		if (HasAnyFlag(state, GfxResourceState::IndexBuffer))  flags |= VK_ACCESS_2_INDEX_READ_BIT;
		if (HasAnyFlag(state, GfxResourceState::IndirectArgs)) flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
		if (HasAnyFlag(state, GfxResourceState::Present))      flags |= VK_ACCESS_2_NONE;
		if (HasAnyFlag(state, GfxResourceState::ASRead))       flags |= VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		if (HasAnyFlag(state, GfxResourceState::ASWrite))      flags |= VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		return flags;
	}

	VkPipelineStageFlags2 ConvertResourceStateToStage(GfxResourceState state)
	{
		VkPipelineStageFlags2 flags = VK_PIPELINE_STAGE_2_NONE;
		if (HasAnyFlag(state, GfxResourceState::RTV))          flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		if (HasAnyFlag(state, GfxResourceState::AllDSV))       flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		if (HasAnyFlag(state, GfxResourceState::VertexSRV))    flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		if (HasAnyFlag(state, GfxResourceState::PixelSRV))     flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		if (HasAnyFlag(state, GfxResourceState::ComputeSRV | GfxResourceState::ComputeUAV)) flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		if (HasAnyFlag(state, GfxResourceState::VertexUAV | GfxResourceState::PixelUAV))   flags |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
		if (HasAnyFlag(state, GfxResourceState::CopyDst | GfxResourceState::CopySrc))      flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		if (HasAnyFlag(state, GfxResourceState::IndexBuffer))  flags |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
		if (HasAnyFlag(state, GfxResourceState::IndirectArgs)) flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
		if (HasAnyFlag(state, GfxResourceState::Present))      flags |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
		if (HasAnyFlag(state, GfxResourceState::ASRead | GfxResourceState::ASWrite)) flags |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
		if (flags == VK_PIPELINE_STAGE_2_NONE) flags = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		return flags;
	}

	VkBlendFactor ConvertBlendFactor(GfxBlend blend)
	{
		switch (blend)
		{
		case GfxBlend::Zero:           return VK_BLEND_FACTOR_ZERO;
		case GfxBlend::One:            return VK_BLEND_FACTOR_ONE;
		case GfxBlend::SrcColor:       return VK_BLEND_FACTOR_SRC_COLOR;
		case GfxBlend::InvSrcColor:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case GfxBlend::SrcAlpha:       return VK_BLEND_FACTOR_SRC_ALPHA;
		case GfxBlend::InvSrcAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case GfxBlend::DstAlpha:       return VK_BLEND_FACTOR_DST_ALPHA;
		case GfxBlend::InvDstAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case GfxBlend::DstColor:       return VK_BLEND_FACTOR_DST_COLOR;
		case GfxBlend::InvDstColor:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case GfxBlend::SrcAlphaSat:    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		case GfxBlend::BlendFactor:    return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case GfxBlend::InvBlendFactor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case GfxBlend::Src1Color:      return VK_BLEND_FACTOR_SRC1_COLOR;
		case GfxBlend::InvSrc1Color:   return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		case GfxBlend::Src1Alpha:      return VK_BLEND_FACTOR_SRC1_ALPHA;
		case GfxBlend::InvSrc1Alpha:   return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
		default: return VK_BLEND_FACTOR_ZERO;
		}
	}

	VkBlendOp ConvertBlendOp(GfxBlendOp op)
	{
		switch (op)
		{
		case GfxBlendOp::Add:        return VK_BLEND_OP_ADD;
		case GfxBlendOp::Subtract:   return VK_BLEND_OP_SUBTRACT;
		case GfxBlendOp::RevSubtract:return VK_BLEND_OP_REVERSE_SUBTRACT;
		case GfxBlendOp::Min:        return VK_BLEND_OP_MIN;
		case GfxBlendOp::Max:        return VK_BLEND_OP_MAX;
		default: return VK_BLEND_OP_ADD;
		}
	}

	VkCompareOp ConvertCompareOp(GfxComparisonFunc func)
	{
		switch (func)
		{
		case GfxComparisonFunc::Never:        return VK_COMPARE_OP_NEVER;
		case GfxComparisonFunc::Less:         return VK_COMPARE_OP_LESS;
		case GfxComparisonFunc::Equal:        return VK_COMPARE_OP_EQUAL;
		case GfxComparisonFunc::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
		case GfxComparisonFunc::Greater:      return VK_COMPARE_OP_GREATER;
		case GfxComparisonFunc::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
		case GfxComparisonFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case GfxComparisonFunc::Always:       return VK_COMPARE_OP_ALWAYS;
		default: return VK_COMPARE_OP_ALWAYS;
		}
	}

	VkStencilOp ConvertStencilOp(GfxStencilOp op)
	{
		switch (op)
		{
		case GfxStencilOp::Keep:    return VK_STENCIL_OP_KEEP;
		case GfxStencilOp::Zero:    return VK_STENCIL_OP_ZERO;
		case GfxStencilOp::Replace: return VK_STENCIL_OP_REPLACE;
		case GfxStencilOp::IncrSat: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case GfxStencilOp::DecrSat: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case GfxStencilOp::Invert:  return VK_STENCIL_OP_INVERT;
		case GfxStencilOp::Incr:    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case GfxStencilOp::Decr:    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
		default: return VK_STENCIL_OP_KEEP;
		}
	}

	VkPolygonMode ConvertFillMode(GfxFillMode mode)
	{
		switch (mode)
		{
		case GfxFillMode::Wireframe: return VK_POLYGON_MODE_LINE;
		case GfxFillMode::Solid:     return VK_POLYGON_MODE_FILL;
		default: return VK_POLYGON_MODE_FILL;
		}
	}

	VkCullModeFlags ConvertCullMode(GfxCullMode mode)
	{
		switch (mode)
		{
		case GfxCullMode::None:  return VK_CULL_MODE_NONE;
		case GfxCullMode::Front: return VK_CULL_MODE_FRONT_BIT;
		case GfxCullMode::Back:  return VK_CULL_MODE_BACK_BIT;
		default: return VK_CULL_MODE_NONE;
		}
	}

	VkPrimitiveTopology ConvertTopology(GfxPrimitiveTopology topology)
	{
		switch (topology)
		{
		case GfxPrimitiveTopology::PointList:    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case GfxPrimitiveTopology::LineList:     return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case GfxPrimitiveTopology::LineStrip:    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case GfxPrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case GfxPrimitiveTopology::TriangleStrip:return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		default:
			if (topology >= GfxPrimitiveTopology::PatchList1)
				return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		}
	}

	VkAttachmentLoadOp ConvertLoadOp(GfxLoadAccessOp op)
	{
		switch (op)
		{
		case GfxLoadAccessOp::Discard:  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		case GfxLoadAccessOp::Preserve: return VK_ATTACHMENT_LOAD_OP_LOAD;
		case GfxLoadAccessOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case GfxLoadAccessOp::NoAccess: return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
		default: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		}
	}

	VkAttachmentStoreOp ConvertStoreOp(GfxStoreAccessOp op)
	{
		switch (op)
		{
		case GfxStoreAccessOp::Discard:  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		case GfxStoreAccessOp::Preserve: return VK_ATTACHMENT_STORE_OP_STORE;
		case GfxStoreAccessOp::Resolve:  return VK_ATTACHMENT_STORE_OP_STORE;
		case GfxStoreAccessOp::NoAccess: return VK_ATTACHMENT_STORE_OP_NONE;
		default: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
	}

	VkQueryType ConvertQueryType(GfxQueryType type)
	{
		switch (type)
		{
		case GfxQueryType::Timestamp:          return VK_QUERY_TYPE_TIMESTAMP;
		case GfxQueryType::Occlusion:          return VK_QUERY_TYPE_OCCLUSION;
		case GfxQueryType::BinaryOcclusion:    return VK_QUERY_TYPE_OCCLUSION;
		case GfxQueryType::PipelineStatistics: return VK_QUERY_TYPE_PIPELINE_STATISTICS;
		default: return VK_QUERY_TYPE_TIMESTAMP;
		}
	}

	VkColorComponentFlags ConvertColorWrite(GfxColorWrite write)
	{
		VkColorComponentFlags flags = 0;
		if ((Uint32)write & (Uint32)GfxColorWrite::EnableRed)   flags |= VK_COLOR_COMPONENT_R_BIT;
		if ((Uint32)write & (Uint32)GfxColorWrite::EnableGreen) flags |= VK_COLOR_COMPONENT_G_BIT;
		if ((Uint32)write & (Uint32)GfxColorWrite::EnableBlue)  flags |= VK_COLOR_COMPONENT_B_BIT;
		if ((Uint32)write & (Uint32)GfxColorWrite::EnableAlpha) flags |= VK_COLOR_COMPONENT_A_BIT;
		if (write == GfxColorWrite::EnableAll) flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		if (write == GfxColorWrite::Disable)   flags = 0;
		return flags;
	}

	VkImageAspectFlags GetAspectFlags(GfxFormat format)
	{
		return GetAspectFlags(ConvertFormat(format));
	}

	VkImageAspectFlags GetAspectFlags(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D16_UNORM:
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		default:
			return VK_IMAGE_ASPECT_COLOR_BIT;
		}
	}
}
