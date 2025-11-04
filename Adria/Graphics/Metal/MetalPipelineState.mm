#import <Metal/Metal.h>
#include "MetalPipelineState.h"
#include "MetalDevice.h"
#include "Graphics/GfxShader.h"
#include "Rendering/ShaderManager.h"

namespace adria
{
    static MTLCompareFunction ConvertCompareFunc(GfxComparisonFunc func)
    {
        switch (func)
        {
        case GfxComparisonFunc::Never: return MTLCompareFunctionNever;
        case GfxComparisonFunc::Less: return MTLCompareFunctionLess;
        case GfxComparisonFunc::Equal: return MTLCompareFunctionEqual;
        case GfxComparisonFunc::LessEqual: return MTLCompareFunctionLessEqual;
        case GfxComparisonFunc::Greater: return MTLCompareFunctionGreater;
        case GfxComparisonFunc::NotEqual: return MTLCompareFunctionNotEqual;
        case GfxComparisonFunc::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        case GfxComparisonFunc::Always: return MTLCompareFunctionAlways;
        default: return MTLCompareFunctionLess;
        }
    }

    MetalGraphicsPipelineState::MetalGraphicsPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc)
        : topology_type(desc.topology_type), cull_mode(desc.rasterizer_state.cull_mode)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = (id<MTLDevice>)metal_device->GetNative();

        MTLRenderPipelineDescriptor* pso_desc = [MTLRenderPipelineDescriptor new];

        id<MTLLibrary> library = metal_device->GetShaderLibrary();
        if (library)
        {
            id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
            id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

            pso_desc.vertexFunction = vertexFunction;
            pso_desc.fragmentFunction = fragmentFunction;
        }

        for (Uint32 i = 0; i < desc.num_render_targets; ++i)
        {
            pso_desc.colorAttachments[i].pixelFormat = MTLPixelFormatBGRA8Unorm; // TODO: Convert from desc.rtv_formats[i]
        }

        if (desc.dsv_format != GfxFormat::UNKNOWN)
        {
            pso_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float; // TODO: Convert from desc.dsv_format
        }

        NSError* error = nil;
        pipeline_state = [device newRenderPipelineStateWithDescriptor:pso_desc error:&error];

        if (error)
        {
            NSLog(@"Failed to create pipeline state: %@", error);
        }

        if (desc.depth_state.depth_enable)
        {
            MTLDepthStencilDescriptor* depth_desc = [MTLDepthStencilDescriptor new];
            depth_desc.depthCompareFunction = ConvertCompareFunc(desc.depth_state.depth_func);
            depth_desc.depthWriteEnabled = desc.depth_state.depth_write_mask == GfxDepthWriteMask::All;
            depth_stencil_state = [device newDepthStencilStateWithDescriptor:depth_desc];
        }
        else
        {
            depth_stencil_state = nil;
        }
    }

    MetalGraphicsPipelineState::~MetalGraphicsPipelineState()
    {
        @autoreleasepool
        {
            pipeline_state = nil;
            depth_stencil_state = nil;
        }
    }

    void* MetalGraphicsPipelineState::GetNative() const
    {
        return (__bridge void*)pipeline_state;
    }

    MetalComputePipelineState::MetalComputePipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = (id<MTLDevice>)metal_device->GetNative();

        id<MTLLibrary> library = metal_device->GetShaderLibrary();
        if (library)
        {
            id<MTLFunction> computeFunction = [library newFunctionWithName:@"compute_main"];

            NSError* error = nil;
            pipeline_state = [device newComputePipelineStateWithFunction:computeFunction error:&error];

            if (error)
            {
                NSLog(@"Failed to create compute pipeline state: %@", error);
            }
        }
    }

    MetalComputePipelineState::~MetalComputePipelineState()
    {
        @autoreleasepool
        {
            pipeline_state = nil;
        }
    }

    void* MetalComputePipelineState::GetNative() const
    {
        return (__bridge void*)pipeline_state;
    }
}
