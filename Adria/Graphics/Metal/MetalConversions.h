#pragma once
#import <Metal/Metal.h>
#include "Graphics/GfxFormat.h"

namespace adria
{
    inline constexpr MTLPixelFormat ToMTLPixelFormat(GfxFormat format)
    {
        switch (format)
        {
        case GfxFormat::UNKNOWN:
            return MTLPixelFormatInvalid;
        case GfxFormat::R32G32B32A32_FLOAT:
            return MTLPixelFormatRGBA32Float;
        case GfxFormat::R32G32B32A32_UINT:
            return MTLPixelFormatRGBA32Uint;
        case GfxFormat::R32G32B32A32_SINT:
            return MTLPixelFormatRGBA32Sint;
        case GfxFormat::R16G16B16A16_FLOAT:
            return MTLPixelFormatRGBA16Float;
        case GfxFormat::R16G16B16A16_UNORM:
            return MTLPixelFormatRGBA16Unorm;
        case GfxFormat::R16G16B16A16_UINT:
            return MTLPixelFormatRGBA16Uint;
        case GfxFormat::R16G16B16A16_SNORM:
            return MTLPixelFormatRGBA16Snorm;
        case GfxFormat::R16G16B16A16_SINT:
            return MTLPixelFormatRGBA16Sint;
        case GfxFormat::R32G32_FLOAT:
            return MTLPixelFormatRG32Float;
        case GfxFormat::R32G32_UINT:
            return MTLPixelFormatRG32Uint;
        case GfxFormat::R32G32_SINT:
            return MTLPixelFormatRG32Sint;
        case GfxFormat::D32_FLOAT_S8X24_UINT:
            return MTLPixelFormatDepth32Float_Stencil8;
        case GfxFormat::R10G10B10A2_UNORM:
            return MTLPixelFormatRGB10A2Unorm;
        case GfxFormat::R10G10B10A2_UINT:
            return MTLPixelFormatRGB10A2Uint;
        case GfxFormat::R11G11B10_FLOAT:
            return MTLPixelFormatRG11B10Float;
        case GfxFormat::R8G8B8A8_UNORM:
            return MTLPixelFormatRGBA8Unorm;
        case GfxFormat::R8G8B8A8_UNORM_SRGB:
            return MTLPixelFormatRGBA8Unorm_sRGB;
        case GfxFormat::R8G8B8A8_UINT:
            return MTLPixelFormatRGBA8Uint;
        case GfxFormat::R8G8B8A8_SNORM:
            return MTLPixelFormatRGBA8Snorm;
        case GfxFormat::R8G8B8A8_SINT:
            return MTLPixelFormatRGBA8Sint;
        case GfxFormat::B8G8R8A8_UNORM:
            return MTLPixelFormatBGRA8Unorm;
        case GfxFormat::B8G8R8A8_UNORM_SRGB:
            return MTLPixelFormatBGRA8Unorm_sRGB;
        case GfxFormat::R16G16_FLOAT:
            return MTLPixelFormatRG16Float;
        case GfxFormat::R16G16_UNORM:
            return MTLPixelFormatRG16Unorm;
        case GfxFormat::R16G16_UINT:
            return MTLPixelFormatRG16Uint;
        case GfxFormat::R16G16_SNORM:
            return MTLPixelFormatRG16Snorm;
        case GfxFormat::R16G16_SINT:
            return MTLPixelFormatRG16Sint;
        case GfxFormat::D32_FLOAT:
            return MTLPixelFormatDepth32Float;
        case GfxFormat::R32_FLOAT:
            return MTLPixelFormatR32Float;
        case GfxFormat::R32_UINT:
            return MTLPixelFormatR32Uint;
        case GfxFormat::R32_SINT:
            return MTLPixelFormatR32Sint;
        case GfxFormat::D24_UNORM_S8_UINT:
            return MTLPixelFormatDepth24Unorm_Stencil8;
        case GfxFormat::R8G8_UNORM:
            return MTLPixelFormatRG8Unorm;
        case GfxFormat::R8G8_UINT:
            return MTLPixelFormatRG8Uint;
        case GfxFormat::R8G8_SNORM:
            return MTLPixelFormatRG8Snorm;
        case GfxFormat::R8G8_SINT:
            return MTLPixelFormatRG8Sint;
        case GfxFormat::D16_UNORM:
            return MTLPixelFormatDepth16Unorm;
        case GfxFormat::R16_FLOAT:
            return MTLPixelFormatR16Float;
        case GfxFormat::R16_UNORM:
            return MTLPixelFormatR16Unorm;
        case GfxFormat::R16_UINT:
            return MTLPixelFormatR16Uint;
        case GfxFormat::R16_SNORM:
            return MTLPixelFormatR16Snorm;
        case GfxFormat::R16_SINT:
            return MTLPixelFormatR16Sint;
        case GfxFormat::R8_UNORM:
            return MTLPixelFormatR8Unorm;
        case GfxFormat::R8_UINT:
            return MTLPixelFormatR8Uint;
        case GfxFormat::R8_SNORM:
            return MTLPixelFormatR8Snorm;
        case GfxFormat::R8_SINT:
            return MTLPixelFormatR8Sint;
        case GfxFormat::BC1_UNORM:
            return MTLPixelFormatBC1_RGBA;
        case GfxFormat::BC1_UNORM_SRGB:
            return MTLPixelFormatBC1_RGBA_sRGB;
        case GfxFormat::BC2_UNORM:
            return MTLPixelFormatBC2_RGBA;
        case GfxFormat::BC2_UNORM_SRGB:
            return MTLPixelFormatBC2_RGBA_sRGB;
        case GfxFormat::BC3_UNORM:
            return MTLPixelFormatBC3_RGBA;
        case GfxFormat::BC3_UNORM_SRGB:
            return MTLPixelFormatBC3_RGBA_sRGB;
        case GfxFormat::BC4_UNORM:
            return MTLPixelFormatBC4_RUnorm;
        case GfxFormat::BC4_SNORM:
            return MTLPixelFormatBC4_RSnorm;
        case GfxFormat::BC5_UNORM:
            return MTLPixelFormatBC5_RGUnorm;
        case GfxFormat::BC5_SNORM:
            return MTLPixelFormatBC5_RGSnorm;
        case GfxFormat::BC6H_UF16:
            return MTLPixelFormatBC6H_RGBUfloat;
        case GfxFormat::BC6H_SF16:
            return MTLPixelFormatBC6H_RGBFloat;
        case GfxFormat::BC7_UNORM:
            return MTLPixelFormatBC7_RGBAUnorm;
        case GfxFormat::BC7_UNORM_SRGB:
            return MTLPixelFormatBC7_RGBAUnorm_sRGB;
        case GfxFormat::R9G9B9E5_SHAREDEXP:
            return MTLPixelFormatRGB9E5Float;
        // Unsupported formats (Metal doesn't support RGB32 or typeless formats)
        case GfxFormat::R32G32B32_FLOAT:
        case GfxFormat::R32G32B32_UINT:
        case GfxFormat::R32G32B32_SINT:
        case GfxFormat::R32G8X24_TYPELESS:
        case GfxFormat::R32_TYPELESS:
        case GfxFormat::R24G8_TYPELESS:
        case GfxFormat::R16_TYPELESS:
            return MTLPixelFormatInvalid;
        default:
            return MTLPixelFormatInvalid;
        }
    }

    inline constexpr MTLIndexType ConvertIndexFormat(GfxFormat format)
    {
        switch (format)
        {
        case GfxFormat::R16_UINT:
            return MTLIndexTypeUInt16;
        case GfxFormat::R32_UINT:
            return MTLIndexTypeUInt32;
        default:
            return MTLIndexTypeUInt32;
        }
    }
}
