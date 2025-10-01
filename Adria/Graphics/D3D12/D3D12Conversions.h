#pragma once
#include "Graphics/GfxResource.h"

namespace adria
{
	inline D3D12_BARRIER_SYNC ToD3D12BarrierSync(GfxResourceState flags)
	{
		using enum GfxResourceState;

		D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;
		Bool const discard = HasFlag(flags, Discard);
		if (!discard && HasFlag(flags, ClearUAV)) sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;

		if (HasFlag(flags, Present))		sync |= D3D12_BARRIER_SYNC_ALL;
		if (HasFlag(flags, Common))			sync |= D3D12_BARRIER_SYNC_ALL;
		if (HasFlag(flags, RTV))			sync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		if (HasAnyFlag(flags, AllDSV))		sync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if (HasAnyFlag(flags, AllVertex))	sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		if (HasAnyFlag(flags, AllPixel))	sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasAnyFlag(flags, AllCompute))	sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		if (HasAnyFlag(flags, AllCopy))		sync |= D3D12_BARRIER_SYNC_COPY;
		if (HasFlag(flags, ShadingRate))	sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if (HasFlag(flags, IndexBuffer))	sync |= D3D12_BARRIER_SYNC_INDEX_INPUT;
		if (HasFlag(flags, IndirectArgs))	sync |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
		if (HasAnyFlag(flags, AllAS))		sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		return sync;
	}
	inline D3D12_BARRIER_LAYOUT ToD3D12BarrierLayout(GfxResourceState flags)
	{
		using enum GfxResourceState;

		if (HasFlag(flags, CopySrc) && HasAnyFlag(flags, AllSRV) && !HasFlag(flags, DSV_ReadOnly)) return D3D12_BARRIER_LAYOUT_GENERIC_READ;
		if (HasFlag(flags, CopyDst) && HasAnyFlag(flags, AllUAV)) return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
		if (HasFlag(flags, DSV_ReadOnly) && (HasAnyFlag(flags, AllSRV) || HasFlag(flags, CopySrc))) return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ;
		if (HasFlag(flags, Discard))		return D3D12_BARRIER_LAYOUT_UNDEFINED;
		if (HasFlag(flags, Present))		return D3D12_BARRIER_LAYOUT_PRESENT;
		if (HasFlag(flags, RTV))			return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		if (HasFlag(flags, DSV))            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		if (HasFlag(flags, DSV_ReadOnly))   return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, AllSRV))		return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
		if (HasAnyFlag(flags, AllUAV))		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasFlag(flags, ClearUAV))		return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
		if (HasFlag(flags, CopyDst))		return D3D12_BARRIER_LAYOUT_COPY_DEST;
		if (HasFlag(flags, CopySrc))		return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
		if (HasFlag(flags, ShadingRate))	return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
		ADRIA_UNREACHABLE();
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}
	inline D3D12_BARRIER_ACCESS ToD3D12BarrierAccess(GfxResourceState flags)
	{
		using enum GfxResourceState;
		if (HasFlag(flags, Discard)) return D3D12_BARRIER_ACCESS_NO_ACCESS;

		D3D12_BARRIER_ACCESS access = D3D12_BARRIER_ACCESS_COMMON;
		if (HasFlag(flags, RTV))             access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if (HasFlag(flags, DSV))             access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if (HasFlag(flags, DSV_ReadOnly))    access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		if (HasAnyFlag(flags, AllSRV))       access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		if (HasAnyFlag(flags, AllUAV))       access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasFlag(flags, ClearUAV))        access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if (HasFlag(flags, CopyDst))         access |= D3D12_BARRIER_ACCESS_COPY_DEST;
		if (HasFlag(flags, CopySrc))         access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if (HasFlag(flags, ShadingRate))     access |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
		if (HasFlag(flags, IndexBuffer))     access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
		if (HasFlag(flags, IndirectArgs))    access |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
		if (HasFlag(flags, ASRead))          access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		if (HasFlag(flags, ASWrite))         access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
		return access;
	}
	inline constexpr D3D12_RESOURCE_STATES ToD3D12LegacyResourceState(GfxResourceState state)
	{
		using enum GfxResourceState;

		D3D12_RESOURCE_STATES api_state = D3D12_RESOURCE_STATE_COMMON;
		if (HasAnyFlag(state, IndexBuffer))		api_state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
		if (HasFlag(state, RTV))				api_state |= D3D12_RESOURCE_STATE_RENDER_TARGET;
		if (HasAnyFlag(state, AllUAV))			api_state |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		if (HasFlag(state, DSV))				api_state |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
		if (HasFlag(state, DSV_ReadOnly))		api_state |= D3D12_RESOURCE_STATE_DEPTH_READ;
		if (HasAnyFlag(state, ComputeSRV | VertexSRV)) api_state |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		if (HasFlag(state, PixelSRV))			api_state |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		if (HasFlag(state, IndirectArgs))		api_state |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
		if (HasFlag(state, CopyDst))			api_state |= D3D12_RESOURCE_STATE_COPY_DEST;
		if (HasFlag(state, CopySrc))			api_state |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		if (HasFlag(state, Present))			api_state |= D3D12_RESOURCE_STATE_PRESENT;
		if (HasAnyFlag(state, AllAS))			api_state |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		return api_state;
	}
	inline Bool CompareByLayout(GfxResourceState flags1, GfxResourceState flags2)
	{
		return ToD3D12BarrierLayout(flags1) == ToD3D12BarrierLayout(flags2);
	}

	inline constexpr DXGI_FORMAT ConvertGfxFormat(GfxFormat _format)
	{
		switch (_format)
		{
		case GfxFormat::UNKNOWN:
			return DXGI_FORMAT_UNKNOWN;
		case GfxFormat::R32G32B32A32_FLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case GfxFormat::R32G32B32A32_UINT:
			return DXGI_FORMAT_R32G32B32A32_UINT;
		case GfxFormat::R32G32B32A32_SINT:
			return DXGI_FORMAT_R32G32B32A32_SINT;
		case GfxFormat::R32G32B32_FLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		case GfxFormat::R32G32B32_UINT:
			return DXGI_FORMAT_R32G32B32_UINT;
		case GfxFormat::R32G32B32_SINT:
			return DXGI_FORMAT_R32G32B32_SINT;
		case GfxFormat::R16G16B16A16_FLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case GfxFormat::R16G16B16A16_UNORM:
			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case GfxFormat::R16G16B16A16_UINT:
			return DXGI_FORMAT_R16G16B16A16_UINT;
		case GfxFormat::R16G16B16A16_SNORM:
			return DXGI_FORMAT_R16G16B16A16_SNORM;
		case GfxFormat::R16G16B16A16_SINT:
			return DXGI_FORMAT_R16G16B16A16_SINT;
		case GfxFormat::R32G32_FLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;
		case GfxFormat::R32G32_UINT:
			return DXGI_FORMAT_R32G32_UINT;
		case GfxFormat::R32G32_SINT:
			return DXGI_FORMAT_R32G32_SINT;
		case GfxFormat::R32G8X24_TYPELESS:
			return DXGI_FORMAT_R32G8X24_TYPELESS;
		case GfxFormat::D32_FLOAT_S8X24_UINT:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		case GfxFormat::R10G10B10A2_UNORM:
			return DXGI_FORMAT_R10G10B10A2_UNORM;
		case GfxFormat::R10G10B10A2_UINT:
			return DXGI_FORMAT_R10G10B10A2_UINT;
		case GfxFormat::R11G11B10_FLOAT:
			return DXGI_FORMAT_R11G11B10_FLOAT;
		case GfxFormat::R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case GfxFormat::R8G8B8A8_UNORM_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case GfxFormat::R8G8B8A8_UINT:
			return DXGI_FORMAT_R8G8B8A8_UINT;
		case GfxFormat::R8G8B8A8_SNORM:
			return DXGI_FORMAT_R8G8B8A8_SNORM;
		case GfxFormat::R8G8B8A8_SINT:
			return DXGI_FORMAT_R8G8B8A8_SINT;
		case GfxFormat::R16G16_FLOAT:
			return DXGI_FORMAT_R16G16_FLOAT;
		case GfxFormat::R16G16_UNORM:
			return DXGI_FORMAT_R16G16_UNORM;
		case GfxFormat::R16G16_UINT:
			return DXGI_FORMAT_R16G16_UINT;
		case GfxFormat::R16G16_SNORM:
			return DXGI_FORMAT_R16G16_SNORM;
		case GfxFormat::R16G16_SINT:
			return DXGI_FORMAT_R16G16_SINT;
		case GfxFormat::R32_TYPELESS:
			return DXGI_FORMAT_R32_TYPELESS;
		case GfxFormat::D32_FLOAT:
			return DXGI_FORMAT_D32_FLOAT;
		case GfxFormat::R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;
		case GfxFormat::R32_UINT:
			return DXGI_FORMAT_R32_UINT;
		case GfxFormat::R32_SINT:
			return DXGI_FORMAT_R32_SINT;
		case GfxFormat::R8G8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;
		case GfxFormat::R8G8_UINT:
			return DXGI_FORMAT_R8G8_UINT;
		case GfxFormat::R8G8_SNORM:
			return DXGI_FORMAT_R8G8_SNORM;
		case GfxFormat::R8G8_SINT:
			return DXGI_FORMAT_R8G8_SINT;
		case GfxFormat::R16_TYPELESS:
			return DXGI_FORMAT_R16_TYPELESS;
		case GfxFormat::R16_FLOAT:
			return DXGI_FORMAT_R16_FLOAT;
		case GfxFormat::D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;
		case GfxFormat::R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
		case GfxFormat::R16_UINT:
			return DXGI_FORMAT_R16_UINT;
		case GfxFormat::R16_SNORM:
			return DXGI_FORMAT_R16_SNORM;
		case GfxFormat::R16_SINT:
			return DXGI_FORMAT_R16_SINT;
		case GfxFormat::R8_UNORM:
			return DXGI_FORMAT_R8_UNORM;
		case GfxFormat::R8_UINT:
			return DXGI_FORMAT_R8_UINT;
		case GfxFormat::R8_SNORM:
			return DXGI_FORMAT_R8_SNORM;
		case GfxFormat::R8_SINT:
			return DXGI_FORMAT_R8_SINT;
		case GfxFormat::BC1_UNORM:
			return DXGI_FORMAT_BC1_UNORM;
		case GfxFormat::BC1_UNORM_SRGB:
			return DXGI_FORMAT_BC1_UNORM_SRGB;
		case GfxFormat::BC2_UNORM:
			return DXGI_FORMAT_BC2_UNORM;
		case GfxFormat::BC2_UNORM_SRGB:
			return DXGI_FORMAT_BC2_UNORM_SRGB;
		case GfxFormat::BC3_UNORM:
			return DXGI_FORMAT_BC3_UNORM;
		case GfxFormat::BC3_UNORM_SRGB:
			return DXGI_FORMAT_BC3_UNORM_SRGB;
		case GfxFormat::BC4_UNORM:
			return DXGI_FORMAT_BC4_UNORM;
		case GfxFormat::BC4_SNORM:
			return DXGI_FORMAT_BC4_SNORM;
		case GfxFormat::BC5_UNORM:
			return DXGI_FORMAT_BC5_UNORM;
		case GfxFormat::BC5_SNORM:
			return DXGI_FORMAT_BC5_SNORM;
		case GfxFormat::B8G8R8A8_UNORM:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		case GfxFormat::B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case GfxFormat::BC6H_UF16:
			return DXGI_FORMAT_BC6H_UF16;
		case GfxFormat::BC6H_SF16:
			return DXGI_FORMAT_BC6H_SF16;
		case GfxFormat::BC7_UNORM:
			return DXGI_FORMAT_BC7_UNORM;
		case GfxFormat::BC7_UNORM_SRGB:
			return DXGI_FORMAT_BC7_UNORM_SRGB;
		case GfxFormat::R9G9B9E5_SHAREDEXP:
			return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
		}
		return DXGI_FORMAT_UNKNOWN;
	}
	inline constexpr GfxFormat ConvertDXGIFormat(DXGI_FORMAT _format)
	{
		switch (_format)
		{
		case DXGI_FORMAT_UNKNOWN:
			return GfxFormat::UNKNOWN;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return GfxFormat::R32G32B32A32_FLOAT;
		case DXGI_FORMAT_R32G32B32A32_UINT:
			return GfxFormat::R32G32B32A32_UINT;
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return GfxFormat::R32G32B32A32_SINT;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			return GfxFormat::R32G32B32_FLOAT;
		case DXGI_FORMAT_R32G32B32_UINT:
			return GfxFormat::R32G32B32_UINT;
		case DXGI_FORMAT_R32G32B32_SINT:
			return GfxFormat::R32G32B32_SINT;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return GfxFormat::R16G16B16A16_FLOAT;
		case DXGI_FORMAT_R16G16B16A16_UNORM:
			return GfxFormat::R16G16B16A16_UNORM;
		case DXGI_FORMAT_R16G16B16A16_UINT:
			return GfxFormat::R16G16B16A16_UINT;
		case DXGI_FORMAT_R16G16B16A16_SNORM:
			return GfxFormat::R16G16B16A16_SNORM;
		case DXGI_FORMAT_R16G16B16A16_SINT:
			return GfxFormat::R16G16B16A16_SINT;
		case DXGI_FORMAT_R32G32_FLOAT:
			return GfxFormat::R32G32_FLOAT;
		case DXGI_FORMAT_R32G32_UINT:
			return GfxFormat::R32G32_UINT;
		case DXGI_FORMAT_R32G32_SINT:
			return GfxFormat::R32G32_SINT;
		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return GfxFormat::R32G8X24_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return GfxFormat::D32_FLOAT_S8X24_UINT;
		case DXGI_FORMAT_R10G10B10A2_UNORM:
			return GfxFormat::R10G10B10A2_UNORM;
		case DXGI_FORMAT_R10G10B10A2_UINT:
			return GfxFormat::R10G10B10A2_UINT;
		case DXGI_FORMAT_R11G11B10_FLOAT:
			return GfxFormat::R11G11B10_FLOAT;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return GfxFormat::R8G8B8A8_UNORM;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			return GfxFormat::R8G8B8A8_UNORM_SRGB;
		case DXGI_FORMAT_R8G8B8A8_UINT:
			return GfxFormat::R8G8B8A8_UINT;
		case DXGI_FORMAT_R8G8B8A8_SNORM:
			return GfxFormat::R8G8B8A8_SNORM;
		case DXGI_FORMAT_R8G8B8A8_SINT:
			return GfxFormat::R8G8B8A8_SINT;
		case DXGI_FORMAT_R16G16_FLOAT:
			return GfxFormat::R16G16_FLOAT;
		case DXGI_FORMAT_R16G16_UNORM:
			return GfxFormat::R16G16_UNORM;
		case DXGI_FORMAT_R16G16_UINT:
			return GfxFormat::R16G16_UINT;
		case DXGI_FORMAT_R16G16_SNORM:
			return GfxFormat::R16G16_SNORM;
		case DXGI_FORMAT_R16G16_SINT:
			return GfxFormat::R16G16_SINT;
		case DXGI_FORMAT_R32_TYPELESS:
			return GfxFormat::R32_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:
			return GfxFormat::D32_FLOAT;
		case DXGI_FORMAT_R32_FLOAT:
			return GfxFormat::R32_FLOAT;
		case DXGI_FORMAT_R32_UINT:
			return GfxFormat::R32_UINT;
		case DXGI_FORMAT_R32_SINT:
			return GfxFormat::R32_SINT;
		case DXGI_FORMAT_R8G8_UNORM:
			return GfxFormat::R8G8_UNORM;
		case DXGI_FORMAT_R8G8_UINT:
			return GfxFormat::R8G8_UINT;
		case DXGI_FORMAT_R8G8_SNORM:
			return GfxFormat::R8G8_SNORM;
		case DXGI_FORMAT_R8G8_SINT:
			return GfxFormat::R8G8_SINT;
		case DXGI_FORMAT_R16_TYPELESS:
			return GfxFormat::R16_TYPELESS;
		case DXGI_FORMAT_R16_FLOAT:
			return GfxFormat::R16_FLOAT;
		case DXGI_FORMAT_D16_UNORM:
			return GfxFormat::D16_UNORM;
		case DXGI_FORMAT_R16_UNORM:
			return GfxFormat::R16_UNORM;
		case DXGI_FORMAT_R16_UINT:
			return GfxFormat::R16_UINT;
		case DXGI_FORMAT_R16_SNORM:
			return GfxFormat::R16_SNORM;
		case DXGI_FORMAT_R16_SINT:
			return GfxFormat::R16_SINT;
		case DXGI_FORMAT_R8_UNORM:
			return GfxFormat::R8_UNORM;
		case DXGI_FORMAT_R8_UINT:
			return GfxFormat::R8_UINT;
		case DXGI_FORMAT_R8_SNORM:
			return GfxFormat::R8_SNORM;
		case DXGI_FORMAT_R8_SINT:
			return GfxFormat::R8_SINT;
		case DXGI_FORMAT_BC1_UNORM:
			return GfxFormat::BC1_UNORM;
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return GfxFormat::BC1_UNORM_SRGB;
		case DXGI_FORMAT_BC2_UNORM:
			return GfxFormat::BC2_UNORM;
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return GfxFormat::BC2_UNORM_SRGB;
		case DXGI_FORMAT_BC3_UNORM:
			return GfxFormat::BC3_UNORM;
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return GfxFormat::BC3_UNORM_SRGB;
		case DXGI_FORMAT_BC4_UNORM:
			return GfxFormat::BC4_UNORM;
		case DXGI_FORMAT_BC4_SNORM:
			return GfxFormat::BC4_SNORM;
		case DXGI_FORMAT_BC5_UNORM:
			return GfxFormat::BC5_UNORM;
		case DXGI_FORMAT_BC5_SNORM:
			return GfxFormat::BC5_SNORM;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return GfxFormat::B8G8R8A8_UNORM;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return GfxFormat::B8G8R8A8_UNORM_SRGB;
		case DXGI_FORMAT_BC6H_UF16:
			return GfxFormat::BC6H_UF16;
		case DXGI_FORMAT_BC6H_SF16:
			return GfxFormat::BC6H_SF16;
		case DXGI_FORMAT_BC7_UNORM:
			return GfxFormat::BC7_UNORM;
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return GfxFormat::BC7_UNORM_SRGB;
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
			return GfxFormat::R9G9B9E5_SHAREDEXP;
		}
		return GfxFormat::UNKNOWN;
	}

	inline D3D12_SHADING_RATE ToD3D12ShadingRate(GfxShadingRate shading_rate)
	{
		switch (shading_rate)
		{
		case GfxShadingRate_1X1: return D3D12_SHADING_RATE_1X1;
		case GfxShadingRate_1X2: return D3D12_SHADING_RATE_1X2;
		case GfxShadingRate_2X1: return D3D12_SHADING_RATE_2X1;
		case GfxShadingRate_2X2: return D3D12_SHADING_RATE_2X2;
		case GfxShadingRate_2X4: return D3D12_SHADING_RATE_2X4;
		case GfxShadingRate_4X2: return D3D12_SHADING_RATE_4X2;
		case GfxShadingRate_4X4: return D3D12_SHADING_RATE_4X4;
		}
		return D3D12_SHADING_RATE_1X1;
	}
	inline D3D12_SHADING_RATE_COMBINER ToD3D12ShadingRateCombiner(GfxShadingRateCombiner shading_rate_combiner)
	{
		switch (shading_rate_combiner)
		{
		case GfxShadingRateCombiner::Passthrough: return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
		case GfxShadingRateCombiner::Override:    return D3D12_SHADING_RATE_COMBINER_OVERRIDE;
		case GfxShadingRateCombiner::Min:		  return D3D12_SHADING_RATE_COMBINER_MIN;
		case GfxShadingRateCombiner::Max:		  return D3D12_SHADING_RATE_COMBINER_MAX;
		case GfxShadingRateCombiner::Sum:		  return D3D12_SHADING_RATE_COMBINER_SUM;
		}
		return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
	}

}