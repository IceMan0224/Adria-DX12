#import <Metal/Metal.h>
#include "MetalPipelineState.h"
#include "MetalDevice.h"
#include "MetalConversions.h"
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

    static MTLStencilOperation ConvertStencilOp(GfxStencilOp op)
    {
        switch (op)
        {
        case GfxStencilOp::Keep: return MTLStencilOperationKeep;
        case GfxStencilOp::Zero: return MTLStencilOperationZero;
        case GfxStencilOp::Replace: return MTLStencilOperationReplace;
        case GfxStencilOp::IncrementSaturate: return MTLStencilOperationIncrementClamp;
        case GfxStencilOp::DecrementSaturate: return MTLStencilOperationDecrementClamp;
        case GfxStencilOp::Invert: return MTLStencilOperationInvert;
        case GfxStencilOp::Increment: return MTLStencilOperationIncrementWrap;
        case GfxStencilOp::Decrement: return MTLStencilOperationDecrementWrap;
        default: return MTLStencilOperationKeep;
        }
    }

    static MTLBlendFactor ConvertBlendFactor(GfxBlend blend)
    {
        switch (blend)
        {
        case GfxBlend::Zero: return MTLBlendFactorZero;
        case GfxBlend::One: return MTLBlendFactorOne;
        case GfxBlend::SrcColor: return MTLBlendFactorSourceColor;
        case GfxBlend::InvSrcColor: return MTLBlendFactorOneMinusSourceColor;
        case GfxBlend::SrcAlpha: return MTLBlendFactorSourceAlpha;
        case GfxBlend::InvSrcAlpha: return MTLBlendFactorOneMinusSourceAlpha;
        case GfxBlend::DestAlpha: return MTLBlendFactorDestinationAlpha;
        case GfxBlend::InvDestAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
        case GfxBlend::DestColor: return MTLBlendFactorDestinationColor;
        case GfxBlend::InvDestColor: return MTLBlendFactorOneMinusDestinationColor;
        case GfxBlend::SrcAlphaSaturate: return MTLBlendFactorSourceAlphaSaturated;
        case GfxBlend::BlendFactor: return MTLBlendFactorBlendColor;
        case GfxBlend::InvBlendFactor: return MTLBlendFactorOneMinusBlendColor;
        case GfxBlend::Src1Color: return MTLBlendFactorSource1Color;
        case GfxBlend::InvSrc1Color: return MTLBlendFactorOneMinusSource1Color;
        case GfxBlend::Src1Alpha: return MTLBlendFactorSource1Alpha;
        case GfxBlend::InvSrc1Alpha: return MTLBlendFactorOneMinusSource1Alpha;
        default: return MTLBlendFactorZero;
        }
    }

    static MTLBlendOperation ConvertBlendOp(GfxBlendOp op)
    {
        switch (op)
        {
        case GfxBlendOp::Add: return MTLBlendOperationAdd;
        case GfxBlendOp::Subtract: return MTLBlendOperationSubtract;
        case GfxBlendOp::RevSubtract: return MTLBlendOperationReverseSubtract;
        case GfxBlendOp::Min: return MTLBlendOperationMin;
        case GfxBlendOp::Max: return MTLBlendOperationMax;
        default: return MTLBlendOperationAdd;
        }
    }

    static MTLColorWriteMask ConvertColorWriteMask(GfxColorWrite mask)
    {
        MTLColorWriteMask result = MTLColorWriteMaskNone;
        if ((mask & GfxColorWrite::Red) != GfxColorWrite::Disable)
            result |= MTLColorWriteMaskRed;
        if ((mask & GfxColorWrite::Green) != GfxColorWrite::Disable)
            result |= MTLColorWriteMaskGreen;
        if ((mask & GfxColorWrite::Blue) != GfxColorWrite::Disable)
            result |= MTLColorWriteMaskBlue;
        if ((mask & GfxColorWrite::Alpha) != GfxColorWrite::Disable)
            result |= MTLColorWriteMaskAlpha;
        return result;
    }

    static MTLCullMode ConvertCullMode(GfxCullMode mode)
    {
        switch (mode)
        {
        case GfxCullMode::None: return MTLCullModeNone;
        case GfxCullMode::Front: return MTLCullModeFront;
        case GfxCullMode::Back: return MTLCullModeBack;
        default: return MTLCullModeNone;
        }
    }

    static MTLPrimitiveTopologyClass ConvertTopologyClass(GfxPrimitiveTopologyType type)
    {
        switch (type)
        {
        case GfxPrimitiveTopologyType::Point:
            return MTLPrimitiveTopologyClassPoint;
        case GfxPrimitiveTopologyType::Line:
            return MTLPrimitiveTopologyClassLine;
        case GfxPrimitiveTopologyType::Triangle:
            return MTLPrimitiveTopologyClassTriangle;
        default:
            return MTLPrimitiveTopologyClassTriangle;
        }
    }

    static id<MTLFunction> GetMetalFunction(GfxDevice* gfx, GfxShaderKey const& shader_key)
    {
        if (!shader_key.IsValid())
        {
            return nil;
        }

        GfxShader const& shader = SM_GetGfxShader(shader_key);
        void* shader_data = shader.GetData();
        Uint64 shader_size = shader.GetSize();

        if (!shader_data || shader_size == 0)
        {
            return nil;
        }

        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        dispatch_data_t data = dispatch_data_create(shader_data, shader_size, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithData:data error:&error];

        if (error || !library)
        {
            if (error)
            {
                ADRIA_LOG(WARNING, "Failed to create Metal library: %s", [[error localizedDescription] UTF8String]);
            }
            return nil;
        }

        NSArray<NSString*>* functionNames = [library functionNames];
        if ([functionNames count] > 0)
        {
            id<MTLFunction> function = [library newFunctionWithName:functionNames[0]];
            return function;
        }

        return nil;
    }

    static id<MTLDepthStencilState> CreateDepthStencilState(id<MTLDevice> device, GfxDepthStencilState const& state)
    {
        MTLDepthStencilDescriptor* depth_desc = [MTLDepthStencilDescriptor new];

        if (state.depth_enable)
        {
            depth_desc.depthCompareFunction = ConvertCompareFunc(state.depth_func);
            depth_desc.depthWriteEnabled = (state.depth_write_mask == GfxDepthWriteMask::All);
        }
        else
        {
            depth_desc.depthCompareFunction = MTLCompareFunctionAlways;
            depth_desc.depthWriteEnabled = NO;
        }

        if (state.stencil_enable)
        {
            MTLStencilDescriptor* front_stencil = [MTLStencilDescriptor new];
            front_stencil.stencilFailureOperation = ConvertStencilOp(state.front_face.stencil_fail_op);
            front_stencil.depthFailureOperation = ConvertStencilOp(state.front_face.stencil_depth_fail_op);
            front_stencil.depthStencilPassOperation = ConvertStencilOp(state.front_face.stencil_pass_op);
            front_stencil.stencilCompareFunction = ConvertCompareFunc(state.front_face.stencil_func);
            front_stencil.readMask = state.stencil_read_mask;
            front_stencil.writeMask = state.stencil_write_mask;

            MTLStencilDescriptor* back_stencil = [MTLStencilDescriptor new];
            back_stencil.stencilFailureOperation = ConvertStencilOp(state.back_face.stencil_fail_op);
            back_stencil.depthFailureOperation = ConvertStencilOp(state.back_face.stencil_depth_fail_op);
            back_stencil.depthStencilPassOperation = ConvertStencilOp(state.back_face.stencil_pass_op);
            back_stencil.stencilCompareFunction = ConvertCompareFunc(state.back_face.stencil_func);
            back_stencil.readMask = state.stencil_read_mask;
            back_stencil.writeMask = state.stencil_write_mask;

            depth_desc.frontFaceStencil = front_stencil;
            depth_desc.backFaceStencil = back_stencil;
        }

        return [device newDepthStencilStateWithDescriptor:depth_desc];
    }

    MetalGraphicsPipelineState::MetalGraphicsPipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc)
        : topology_type(desc.topology_type)
        , cull_mode(desc.rasterizer_state.cull_mode)
        , front_counter_clockwise(desc.rasterizer_state.front_counter_clockwise)
        , depth_bias(static_cast<Float>(desc.rasterizer_state.depth_bias))
        , slope_scaled_depth_bias(desc.rasterizer_state.slope_scaled_depth_bias)
        , depth_bias_clamp(desc.rasterizer_state.depth_bias_clamp)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        MTLRenderPipelineDescriptor* pso_desc = [MTLRenderPipelineDescriptor new];

        id<MTLFunction> vertex_func = GetMetalFunction(gfx, desc.VS);
        id<MTLFunction> fragment_func = GetMetalFunction(gfx, desc.PS);

        if (!vertex_func)
        {
            ADRIA_LOG(ERROR, "Failed to create vertex function for graphics pipeline");
            return;
        }

        pso_desc.vertexFunction = vertex_func;
        pso_desc.fragmentFunction = fragment_func; 

        for (Uint32 i = 0; i < desc.num_render_targets; ++i)
        {
            if (desc.rtv_formats[i] == GfxFormat::UNKNOWN)
                continue;

            MTLRenderPipelineColorAttachmentDescriptor* color_attachment = pso_desc.colorAttachments[i];
            color_attachment.pixelFormat = ToMTLPixelFormat(desc.rtv_formats[i]);

            GfxRasterizerState::GfxRenderTargetBlendState const& blend_state =
                desc.blend_state.independent_blend_enable ? desc.blend_state.render_target[i] : desc.blend_state.render_target[0];

            color_attachment.blendingEnabled = blend_state.blend_enable;
            color_attachment.sourceRGBBlendFactor = ConvertBlendFactor(blend_state.src_blend);
            color_attachment.destinationRGBBlendFactor = ConvertBlendFactor(blend_state.dest_blend);
            color_attachment.rgbBlendOperation = ConvertBlendOp(blend_state.blend_op);
            color_attachment.sourceAlphaBlendFactor = ConvertBlendFactor(blend_state.src_blend_alpha);
            color_attachment.destinationAlphaBlendFactor = ConvertBlendFactor(blend_state.dest_blend_alpha);
            color_attachment.alphaBlendOperation = ConvertBlendOp(blend_state.blend_op_alpha);
            color_attachment.writeMask = ConvertColorWriteMask(blend_state.render_target_write_mask);
        }

        if (desc.dsv_format != GfxFormat::UNKNOWN)
        {
            pso_desc.depthAttachmentPixelFormat = ToMTLPixelFormat(desc.dsv_format);

            if (desc.dsv_format == GfxFormat::D24_UNORM_S8_UINT ||
                desc.dsv_format == GfxFormat::D32_FLOAT_S8X24_UINT)
            {
                pso_desc.stencilAttachmentPixelFormat = ToMTLPixelFormat(desc.dsv_format);
            }
        }

        pso_desc.inputPrimitiveTopology = ConvertTopologyClass(desc.topology_type);
        pso_desc.alphaToCoverageEnabled = desc.blend_state.alpha_to_coverage_enable;
        pso_desc.rasterSampleCount = 1; 

        NSError* error = nil;
        pipeline_state = [device newRenderPipelineStateWithDescriptor:pso_desc error:&error];
        if (error || !pipeline_state)
        {
            if (error)
            {
                ADRIA_LOG(ERROR, "Failed to create graphics pipeline state: %s", [[error localizedDescription] UTF8String]);
            }
            return;
        }
        depth_stencil_state = CreateDepthStencilState(device, desc.depth_state);
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

    MetalMeshShadingPipelineState::MetalMeshShadingPipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc)
        : cull_mode(desc.rasterizer_state.cull_mode)
        , front_counter_clockwise(desc.rasterizer_state.front_counter_clockwise)
        , depth_bias(static_cast<Float>(desc.rasterizer_state.depth_bias))
        , slope_scaled_depth_bias(desc.rasterizer_state.slope_scaled_depth_bias)
        , depth_bias_clamp(desc.rasterizer_state.depth_bias_clamp)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        if (![device supportsFamily:MTLGPUFamilyApple7])
        {
            ADRIA_LOG(ERROR, "Mesh shaders require MTLGPUFamilyApple7 or later");
            return;
        }

        MTLMeshRenderPipelineDescriptor* pso_desc = [MTLMeshRenderPipelineDescriptor new];

        id<MTLFunction> object_func = GetMetalFunction(gfx, desc.AS);
        id<MTLFunction> mesh_func = GetMetalFunction(gfx, desc.MS);  
        id<MTLFunction> fragment_func = GetMetalFunction(gfx, desc.PS); 

        if (!mesh_func)
        {
            ADRIA_LOG(ERROR, "Failed to create mesh function for mesh shading pipeline");
            return;
        }

        pso_desc.objectFunction = object_func;
        pso_desc.meshFunction = mesh_func;
        pso_desc.fragmentFunction = fragment_func;

        if (object_func)
        {
            threads_per_object_threadgroup = MTLSizeMake(
                object_func.maxTotalThreadsPerThreadgroup > 0 ? object_func.maxTotalThreadsPerThreadgroup : 32,
                1, 1
            );
        }
        else
        {
            threads_per_object_threadgroup = MTLSizeMake(0, 0, 0);
        }

        threads_per_mesh_threadgroup = MTLSizeMake(
            mesh_func.maxTotalThreadsPerThreadgroup > 0 ? mesh_func.maxTotalThreadsPerThreadgroup : 32,
            1, 1
        );

        for (Uint32 i = 0; i < desc.num_render_targets; ++i)
        {
            if (desc.rtv_formats[i] == GfxFormat::UNKNOWN)
                continue;

            MTLRenderPipelineColorAttachmentDescriptor* color_attachment = pso_desc.colorAttachments[i];
            color_attachment.pixelFormat = ToMTLPixelFormat(desc.rtv_formats[i]);

            GfxBlendState::GfxRenderTargetBlendState const& blend_state =
                desc.blend_state.independent_blend_enable ? desc.blend_state.render_target[i] : desc.blend_state.render_target[0];

            color_attachment.blendingEnabled = blend_state.blend_enable;
            color_attachment.sourceRGBBlendFactor = ConvertBlendFactor(blend_state.src_blend);
            color_attachment.destinationRGBBlendFactor = ConvertBlendFactor(blend_state.dest_blend);
            color_attachment.rgbBlendOperation = ConvertBlendOp(blend_state.blend_op);
            color_attachment.sourceAlphaBlendFactor = ConvertBlendFactor(blend_state.src_blend_alpha);
            color_attachment.destinationAlphaBlendFactor = ConvertBlendFactor(blend_state.dest_blend_alpha);
            color_attachment.alphaBlendOperation = ConvertBlendOp(blend_state.blend_op_alpha);
            color_attachment.writeMask = ConvertColorWriteMask(blend_state.render_target_write_mask);
        }

        if (desc.dsv_format != GfxFormat::UNKNOWN)
        {
            pso_desc.depthAttachmentPixelFormat = ToMTLPixelFormat(desc.dsv_format);

            if (desc.dsv_format == GfxFormat::D24_UNORM_S8_UINT ||
                desc.dsv_format == GfxFormat::D32_FLOAT_S8X24_UINT)
            {
                pso_desc.stencilAttachmentPixelFormat = ToMTLPixelFormat(desc.dsv_format);
            }
        }

        pso_desc.alphaToCoverageEnabled = desc.blend_state.alpha_to_coverage_enable;
        pso_desc.rasterSampleCount = 1;

        NSError* error = nil;
        pipeline_state = [device newRenderPipelineStateWithDescriptor:pso_desc error:&error];

        if (error || !pipeline_state)
        {
            if (error)
            {
                ADRIA_LOG(ERROR, "Failed to create mesh shading pipeline state: %s", [[error localizedDescription] UTF8String]);
            }
            return;
        }
        depth_stencil_state = CreateDepthStencilState(device, desc.depth_state);
    }

    MetalMeshShadingPipelineState::~MetalMeshShadingPipelineState()
    {
        @autoreleasepool
        {
            pipeline_state = nil;
            depth_stencil_state = nil;
        }
    }

    void* MetalMeshShadingPipelineState::GetNative() const
    {
        return (__bridge void*)pipeline_state;
    }

    MetalComputePipelineState::MetalComputePipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc)
    {
        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();
        id<MTLFunction> compute_func = GetMetalFunction(gfx, desc.CS);

        if (!compute_func)
        {
            ADRIA_LOG(ERROR, "Failed to create compute function for compute pipeline");
            return;
        }

        NSError* error = nil;
        pipeline_state = [device newComputePipelineStateWithFunction:compute_func error:&error];

        if (error || !pipeline_state)
        {
            if (error)
            {
                ADRIA_LOG(ERROR, "Failed to create compute pipeline state: %s", [[error localizedDescription] UTF8String]);
            }
            return;
        }

        NSUInteger max_threads = compute_func.maxTotalThreadsPerThreadgroup;
        NSUInteger thread_width = pipeline_state.threadExecutionWidth;
        if (max_threads >= 1024)
        {
            threads_per_threadgroup = MTLSizeMake(32, 32, 1); 
        }
        else if (max_threads >= 512)
        {
            threads_per_threadgroup = MTLSizeMake(16, 32, 1); 
        }
        else if (max_threads >= 256)
        {
            threads_per_threadgroup = MTLSizeMake(16, 16, 1); 
        }
        else
        {
            threads_per_threadgroup = MTLSizeMake(8, 8, 1); 
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
