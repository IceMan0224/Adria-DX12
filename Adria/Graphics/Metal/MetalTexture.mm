#import <Metal/Metal.h>
#include "MetalTexture.h"
#include "MetalDevice.h"
#include "MetalConversions.h"
#include "Graphics/GfxFormat.h"
#include "Utilities/Enum.h"

namespace adria
{
    MetalTexture::MetalTexture(GfxDevice* gfx, GfxTextureDesc const& desc)
        : GfxTexture(gfx, desc), owns_texture(true)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        MTLTextureDescriptor* tex_desc = [MTLTextureDescriptor new];
        tex_desc.width = desc.width;
        tex_desc.height = desc.height;
        tex_desc.pixelFormat = ToMTLPixelFormat(desc.format);
        tex_desc.usage = MTLTextureUsageShaderRead;

        if (HasFlag(desc.bind_flags, GfxBindFlag::RenderTarget))
            tex_desc.usage |= MTLTextureUsageRenderTarget;
        if (HasFlag(desc.bind_flags, GfxBindFlag::DepthStencil))
            tex_desc.usage |= MTLTextureUsageRenderTarget;
        if (HasFlag(desc.bind_flags, GfxBindFlag::UnorderedAccess))
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
        }

        tex_desc.mipmapLevelCount = desc.mip_levels;
        tex_desc.arrayLength = desc.array_size;

        metal_texture = [device newTextureWithDescriptor:tex_desc];
    }

    MetalTexture::MetalTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data)
        : MetalTexture(gfx, desc)
    {
        if (data.sub_data && data.sub_count > 0)
        {
            // Upload first subresource for now (could be extended to handle all subresources)
            MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
            [metal_texture replaceRegion:region
                             mipmapLevel:0
                               withBytes:data.sub_data[0].data
                             bytesPerRow:data.sub_data[0].row_pitch];
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

    Uint64 MetalTexture::GetGpuAddress() const
    {
        return 0; // Metal textures don't have GPU addresses like buffers do
    }

    void* MetalTexture::Map()
    {
        return nullptr; // Metal textures cannot be directly mapped
    }

    void MetalTexture::Unmap()
    {
        // No-op for Metal textures
    }

    void* MetalTexture::GetSharedHandle() const
    {
        return nullptr;
    }

    Uint32 MetalTexture::GetRowPitch(Uint32 mip_level) const
    {
        // Calculate row pitch based on format and width
        Uint32 mip_width = std::max(1u, desc.width >> mip_level);
        return mip_width * GetGfxFormatStride(desc.format);
    }

    void MetalTexture::SetName(Char const* name)
    {
        [metal_texture setLabel:[NSString stringWithUTF8String:name]];
    }

    void MetalTexture::UpdateHandle(void* metal_texture_handle)
    {
        ADRIA_ASSERT(!owns_texture && "UpdateHandle should only be called on backbuffer textures!");
        metal_texture = (__bridge id<MTLTexture>)metal_texture_handle;
    }
}
