#pragma once
#include "Graphics/GfxResource.h"
#include "Graphics/GfxDescriptor.h"
#include "Graphics/GfxShadingRate.h"

namespace adria
{
	D3D12_BARRIER_SYNC ToD3D12BarrierSync(GfxResourceState flags);
	D3D12_BARRIER_LAYOUT ToD3D12BarrierLayout(GfxResourceState flags);
	D3D12_BARRIER_ACCESS ToD3D12BarrierAccess(GfxResourceState flags);
	D3D12_RESOURCE_STATES ToD3D12LegacyResourceState(GfxResourceState state);

	inline Bool CompareByLayout(GfxResourceState flags1, GfxResourceState flags2)
	{
		return ToD3D12BarrierLayout(flags1) == ToD3D12BarrierLayout(flags2);
	}

	inline constexpr DXGI_FORMAT ToDXGIFormat(GfxFormat _format)
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
	inline constexpr GfxFormat FromDXGIFormat(DXGI_FORMAT _format)
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

	inline constexpr D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12HeapType(GfxDescriptorType type)
	{
		switch (type)
		{
		case GfxDescriptorType::CBV_SRV_UAV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		case GfxDescriptorType::Sampler:
			return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		case GfxDescriptorType::RTV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		case GfxDescriptorType::DSV:
			return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		}
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	}
}