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
        tex_desc.usage = MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;

        if (HasFlag(desc.bind_flags, GfxBindFlag::RenderTarget))
            tex_desc.usage |= MTLTextureUsageRenderTarget;
        if (HasFlag(desc.bind_flags, GfxBindFlag::DepthStencil))
            tex_desc.usage |= MTLTextureUsageRenderTarget;
        if (HasFlag(desc.bind_flags, GfxBindFlag::UnorderedAccess))
            tex_desc.usage |= MTLTextureUsageShaderWrite;

        switch (desc.type)
        {
        case GfxTextureType_1D:
            if (desc.array_size > 1)
            {
                tex_desc.textureType = MTLTextureType1DArray;
            }
            else
            {
                tex_desc.textureType = MTLTextureType1D;
            }
            break;
        case GfxTextureType_2D:
            if (HasFlag(desc.misc_flags, GfxTextureMiscFlag::TextureCube))
            {
                if (desc.array_size == 6)
                {
                    tex_desc.textureType = MTLTextureTypeCube;
                }
                else
                {
                    tex_desc.textureType = MTLTextureTypeCubeArray;
                }
            }
            else if (desc.array_size > 1)
            {
                tex_desc.textureType = MTLTextureType2DArray;
            }
            else
            {
                tex_desc.textureType = MTLTextureType2D;
            }
            break;
        case GfxTextureType_3D:
            tex_desc.textureType = MTLTextureType3D;
            tex_desc.depth = desc.depth;
            break;
        }

        tex_desc.mipmapLevelCount = desc.mip_levels;
        if (HasFlag(desc.misc_flags, GfxTextureMiscFlag::TextureCube))
        {
            tex_desc.arrayLength = desc.array_size / 6;
        }
        else
        {
            tex_desc.arrayLength = desc.array_size;
        }

        metal_texture = [device newTextureWithDescriptor:tex_desc];

        if (metal_texture)
        {
            metal_device->MakeResident(metal_texture);
        }
    }

    
    MetalTexture::MetalTexture(GfxDevice* gfx, GfxTextureDesc const& desc, GfxTextureData const& data)
        : MetalTexture(gfx, desc)
    {
        if (data.sub_data && data.sub_count > 0)
        {
            Uint32 mip_levels = desc.mip_levels;
            Uint32 array_size = desc.array_size;
            
            for (Uint32 slice = 0; slice < array_size; ++slice)
            {
                Uint32 mip_width = desc.width;
                Uint32 mip_height = desc.height;
                Uint32 mip_depth = std::max(1u, desc.depth);
                
                for (Uint32 mip = 0; mip < mip_levels; ++mip)
                {
                    Uint32 sub_resource_index = slice * mip_levels + mip;
                    
                    if (sub_resource_index >= data.sub_count)
                        break;
                    
                    GfxTextureSubData const& sub_data = data.sub_data[sub_resource_index];
                    
                    if (sub_data.data)
                    {
                        MTLRegion region;
                        if (desc.type == GfxTextureType_3D)
                        {
                            region = MTLRegionMake3D(0, 0, 0, mip_width, mip_height, mip_depth);
                        }
                        else
                        {
                            region = MTLRegionMake2D(0, 0, mip_width, mip_height);
                        }
                        
                        [metal_texture replaceRegion:region
                                        mipmapLevel:mip
                                            slice:slice
                                        withBytes:sub_data.data
                                        bytesPerRow:sub_data.row_pitch
                                    bytesPerImage:sub_data.slice_pitch];
                    }
                    
                    mip_width = std::max(1u, mip_width / 2);
                    mip_height = std::max(1u, mip_height / 2);
                    if (desc.type == GfxTextureType_3D)
                    {
                        mip_depth = std::max(1u, mip_depth / 2);
                    }
                }
            }
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
            MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
            metal_device->Evict(metal_texture);
            metal_texture = nil;
        }
    }

    void* MetalTexture::GetNative() const
    {
        return (__bridge void*)metal_texture;
    }

    Uint64 MetalTexture::GetGpuAddress() const
    {
        return 0; 
    }

    void* MetalTexture::Map()
    {
        return nullptr; 
    }

    void MetalTexture::Unmap()
    {
        
    }

    void* MetalTexture::GetSharedHandle() const
    {
        return nullptr;
    }

    Uint32 MetalTexture::GetRowPitch(Uint32 mip_level) const
    {
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

        if (metal_texture)
        {
            desc.width = [metal_texture width];
            desc.height = [metal_texture height];
        }
    }
}
