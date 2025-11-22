#import <Metal/Metal.h>
#include "MetalPipelineState.h"
#include "MetalDevice.h"
#include "MetalConversions.h"
#include "MetalShaderReflection.h"
#include "Graphics/GfxShader.h"
#include "Rendering/ShaderManager.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);
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
        case GfxStencilOp::IncrSat: return MTLStencilOperationIncrementClamp;
        case GfxStencilOp::DecrSat: return MTLStencilOperationDecrementClamp;
        case GfxStencilOp::Invert: return MTLStencilOperationInvert;
        case GfxStencilOp::Incr: return MTLStencilOperationIncrementWrap;
        case GfxStencilOp::Decr: return MTLStencilOperationDecrementWrap;
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
        case GfxBlend::DstAlpha: return MTLBlendFactorDestinationAlpha;
        case GfxBlend::InvDstAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
        case GfxBlend::DstColor: return MTLBlendFactorDestinationColor;
        case GfxBlend::InvDstColor: return MTLBlendFactorOneMinusDestinationColor;
        case GfxBlend::SrcAlphaSat: return MTLBlendFactorSourceAlphaSaturated;
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
        Uint32 mask_value = static_cast<Uint32>(mask);
        if (mask_value & static_cast<Uint32>(GfxColorWrite::EnableRed))
            result |= MTLColorWriteMaskRed;
        if (mask_value & static_cast<Uint32>(GfxColorWrite::EnableGreen))
            result |= MTLColorWriteMaskGreen;
        if (mask_value & static_cast<Uint32>(GfxColorWrite::EnableBlue))
            result |= MTLColorWriteMaskBlue;
        if (mask_value & static_cast<Uint32>(GfxColorWrite::EnableAlpha))
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

    // Map HLSL semantics to Metal attribute indices
    // Metal IR converter assigns attributes sequentially starting from index 11
    static Uint32 GetMetalAttributeIndex(Uint32 input_element_index)
    {
        return 11 + input_element_index;
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

        // Shader data is pure metallib (no header to skip)
        Uint8 const* metallib_data = (Uint8 const*)shader_data;
        Uint64 metallib_size = shader_size;

        dispatch_data_t data = dispatch_data_create(metallib_data, metallib_size, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
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
        @autoreleasepool
        {
            MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
            id<MTLDevice> device = metal_device->GetMTLDevice();

            // Geometry shaders are not supported on Metal
            // (Emulation via Metal Shader Converter requires complex runtime linking)
            if (desc.GS.IsValid())
            {
                ADRIA_LOG(ERROR, "Geometry shaders are not supported on Metal backend");
                return;
            }

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

        // Setup vertex descriptor if input layout is specified
        if (!desc.input_layout.elements.empty())
        {
            MTLVertexDescriptor* vertex_desc = [MTLVertexDescriptor new];

            for (Uint32 i = 0; i < desc.input_layout.elements.size(); ++i)
            {
                GfxInputLayout::GfxInputElement const& element = desc.input_layout.elements[i];

                // Map to Metal attribute index (Metal IR converter uses sequential indices starting from 11)
                Uint32 metal_attr_index = GetMetalAttributeIndex(i);

                MTLVertexAttributeDescriptor* attr = vertex_desc.attributes[metal_attr_index];
                attr.format = ToMTLVertexFormat(element.format);
                attr.offset = element.aligned_byte_offset;
                attr.bufferIndex = element.input_slot;
            }

            // Setup buffer layouts
            // Collect unique buffer slots and calculate strides
            std::map<Uint32, Uint32> buffer_strides;
            for (auto const& element : desc.input_layout.elements)
            {
                Uint32 slot = element.input_slot;
                Uint32 element_end = element.aligned_byte_offset;

                switch (element.format)
                {
                case GfxFormat::R32G32B32A32_FLOAT:
                case GfxFormat::R32G32B32A32_UINT:
                case GfxFormat::R32G32B32A32_SINT:
                    element_end += 16;
                    break;
                case GfxFormat::R32G32B32_FLOAT:
                case GfxFormat::R32G32B32_UINT:
                case GfxFormat::R32G32B32_SINT:
                    element_end += 12;
                    break;
                case GfxFormat::R32G32_FLOAT:
                case GfxFormat::R32G32_UINT:
                case GfxFormat::R32G32_SINT:
                    element_end += 8;
                    break;
                case GfxFormat::R32_FLOAT:
                case GfxFormat::R32_UINT:
                case GfxFormat::R32_SINT:
                    element_end += 4;
                    break;
                case GfxFormat::R16G16B16A16_FLOAT:
                case GfxFormat::R16G16B16A16_UNORM:
                case GfxFormat::R16G16B16A16_UINT:
                case GfxFormat::R16G16B16A16_SNORM:
                case GfxFormat::R16G16B16A16_SINT:
                    element_end += 8;
                    break;
                case GfxFormat::R16G16_FLOAT:
                case GfxFormat::R16G16_UNORM:
                case GfxFormat::R16G16_UINT:
                case GfxFormat::R16G16_SNORM:
                case GfxFormat::R16G16_SINT:
                    element_end += 4;
                    break;
                case GfxFormat::R8G8B8A8_UNORM:
                case GfxFormat::R8G8B8A8_UINT:
                case GfxFormat::R8G8B8A8_SNORM:
                case GfxFormat::R8G8B8A8_SINT:
                    element_end += 4;
                    break;
                default:
                    break;
                }

                buffer_strides[slot] = std::max(buffer_strides[slot], element_end);
            }

            for (auto const& [slot, stride] : buffer_strides)
            {
                MTLVertexBufferLayoutDescriptor* layout = vertex_desc.layouts[slot];
                layout.stride = stride;
                layout.stepRate = 1;
                layout.stepFunction = MTLVertexStepFunctionPerVertex;
            }

            pso_desc.vertexDescriptor = vertex_desc;
        }

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

            // Only create depth stencil state if we have a depth attachment
            if (desc.dsv_format != GfxFormat::UNKNOWN)
            {
                depth_stencil_state = CreateDepthStencilState(device, desc.depth_state);
            }
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

    MetalMeshShadingPipelineState::MetalMeshShadingPipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc)
        : cull_mode(desc.rasterizer_state.cull_mode)
        , front_counter_clockwise(desc.rasterizer_state.front_counter_clockwise)
        , depth_bias(static_cast<Float>(desc.rasterizer_state.depth_bias))
        , slope_scaled_depth_bias(desc.rasterizer_state.slope_scaled_depth_bias)
        , depth_bias_clamp(desc.rasterizer_state.depth_bias_clamp)
    {
        @autoreleasepool
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
            // Default threadgroup size for object shaders
            threads_per_object_threadgroup = MTLSizeMake(32, 1, 1);
        }
        else
        {
            threads_per_object_threadgroup = MTLSizeMake(0, 0, 0);
        }

        threads_per_mesh_threadgroup = MTLSizeMake(32, 1, 1);

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
        pipeline_state = [device newRenderPipelineStateWithMeshDescriptor:pso_desc options:MTLPipelineOptionNone reflection:nil error:&error];

        if (error || !pipeline_state)
        {
            if (error)
            {
                ADRIA_LOG(ERROR, "Failed to create mesh shading pipeline state: %s", [[error localizedDescription] UTF8String]);
            }
            return;
        }

            // Only create depth stencil state if we have a depth attachment
            if (desc.dsv_format != GfxFormat::UNKNOWN)
            {
                depth_stencil_state = CreateDepthStencilState(device, desc.depth_state);
            }
        }
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
        @autoreleasepool
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

            // Read threadgroup size from reflection data stored separately
            GfxShader const& shader = SM_GetGfxShader(desc.CS);
            void const* reflection_data = shader.GetReflectionData();

            if (reflection_data && shader.GetReflectionSize() >= sizeof(MetalShaderReflection))
            {
                MetalShaderReflection const* reflection = (MetalShaderReflection const*)reflection_data;
                threads_per_threadgroup = MTLSizeMake(
                    reflection->threadsPerThreadgroup[0],
                    reflection->threadsPerThreadgroup[1],
                    reflection->threadsPerThreadgroup[2]
                );

                ADRIA_LOG(INFO, "Compute shader threadgroup: %lux%lux%lu",
                          threads_per_threadgroup.width,
                          threads_per_threadgroup.height,
                          threads_per_threadgroup.depth);
            }
            else
            {
                ADRIA_LOG(WARNING, "Shader missing reflection data, using default threadgroup size");
                threads_per_threadgroup = MTLSizeMake(8, 8, 1);
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
