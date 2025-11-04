#import <Metal/Metal.h>
#include "MetalTexture.h"
#include "MetalDevice.h"
#include "Graphics/GfxFormat.h"

namespace adria
{
    static MTLPixelFormat ConvertFormat(GfxFormat format)
    {
        switch (format)
        {
        case GfxFormat::R8G8B8A8_UNORM: return MTLPixelFormatRGBA8Unorm;
        case GfxFormat::B8G8R8A8_UNORM: return MTLPixelFormatBGRA8Unorm;
        case GfxFormat::R16G16B16A16_FLOAT: return MTLPixelFormatRGBA16Float;
        case GfxFormat::R32G32B32A32_FLOAT: return MTLPixelFormatRGBA32Float;
        case GfxFormat::R32_FLOAT: return MTLPixelFormatR32Float;
        case GfxFormat::D32_FLOAT: return MTLPixelFormatDepth32Float;
        case GfxFormat::D24_UNORM_S8_UINT: return MTLPixelFormatDepth24Unorm_Stencil8;
        case GfxFormat::R16_FLOAT: return MTLPixelFormatR16Float;
        default: return MTLPixelFormatRGBA8Unorm;
        }
    }

    MetalTexture::MetalTexture(GfxDevice* gfx, GfxTextureDesc const& desc)
        : GfxTexture(gfx, desc), owns_texture(true)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = (id<MTLDevice>)metal_device->GetNative();

        MTLTextureDescriptor* tex_desc = [MTLTextureDescriptor new];
        tex_desc.width = desc.width;
        tex_desc.height = desc.height;
        tex_desc.pixelFormat = ConvertFormat(desc.format);
        tex_desc.usage = MTLTextureUsageShaderRead;

        if (desc.bind_flags & GfxBindFlag::RenderTarget)
            tex_desc.usage |= MTLTextureUsageRenderTarget;
        if (desc.bind_flags & GfxBindFlag::DepthStencil)
            tex_desc.usage |= MTLTextureUsageRenderTarget;
        if (desc.bind_flags & GfxBindFlag::UnorderedAccess)
            tex_desc.usage |= MTLTextureUsageShaderWrite;

        switch (desc.type)
        {
        case GfxTextureType_1D:
            tex_desc.textureType = MTLTextureType1D;
            break;
        case GfxTextureType_2D:
            tex_desc.textureType = MTLTextureType2D;
            break;
        case GfxTextureType_3D:
            tex_desc.textureType = MTLTextureType3D;
            tex_desc.depth = desc.depth;
            break;
        case GfxTextureType_Cube:
            tex_desc.textureType = MTLTextureTypeCube;
            break;
        }

        tex_desc.mipmapLevelCount = desc.mip_levels;
        tex_desc.arrayLength = desc.array_size;

        metal_texture = [device newTextureWithDescriptor:tex_desc];
    }

    MetalTexture::MetalTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data)
        : MetalTexture(gfx, desc)
    {
        if (data.data && data.size > 0)
        {
            MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
            [metal_texture replaceRegion:region
                             mipmapLevel:0
                               withBytes:data.data
                             bytesPerRow:data.row_pitch];
        }
    }

    MetalTexture::MetalTexture(GfxDevice* gfx, void* metal_texture_ptr, GfxTextureDesc const& desc)
        : GfxTexture(gfx, desc), owns_texture(false)
    {
        metal_texture = (__bridge id<MTLTexture>)metal_texture_ptr;
    }

    MetalTexture::~MetalTexture()
    {
        if (owns_texture && metal_texture)
        {
            metal_texture = nil;
        }
    }

    void* MetalTexture::GetNative() const
    {
        return (__bridge void*)metal_texture;
    }

    void* MetalTexture::GetSharedHandle() const
    {
        return nullptr;
    }

    void MetalTexture::SetName(Char const* name)
    {
        [metal_texture setLabel:[NSString stringWithUTF8String:name]];
    }
}
