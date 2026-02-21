#import <Metal/Metal.h>
#include <metal_irconverter/metal_irconverter.h>
#include "MetalRayTracingPipeline.h"
#include "MetalDevice.h"
#include "MetalShaderReflection.h"
#include "Utilities/StringConversions.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);

    static constexpr Char const* kRaygenIndirectionKernelName = "RaygenIndirection";
    static constexpr Char const* kIndirectTriangleIntersectionFunctionName = "irconverter.wrapper.intersection.function.triangle";
    static id<MTLLibrary> SynthesizeIndirectRayDispatchLibrary(id<MTLDevice> device)
    {
        IRCompiler* compiler = IRCompilerCreate();
        IRCompilerSetMinimumDeploymentTarget(compiler, IROperatingSystem_macOS, "15.0.0");
        IRMetalLibBinary* metallib = IRMetalLibBinaryCreate();
        bool success = IRMetalLibSynthesizeIndirectRayDispatchFunction(compiler, metallib);
        if (!success)
        {
            ADRIA_LOG(ERROR, "Failed to synthesize indirect ray dispatch function");
            IRMetalLibBinaryDestroy(metallib);
            IRCompilerDestroy(compiler);
            return nil;
        }
        dispatch_data_t data = IRMetalLibGetBytecodeData(metallib);
        NSError* error = nil;
        id<MTLLibrary> lib = [device newLibraryWithData:data error:&error];
        if (error || !lib)
        {
            ADRIA_LOG(ERROR, "Failed to create dispatch library: %s",
                     error ? [[error localizedDescription] UTF8String] : "unknown");
        }
        IRMetalLibBinaryDestroy(metallib);
        IRCompilerDestroy(compiler);
        return lib;
    }

    static id<MTLLibrary> SynthesizeIndirectTriangleIntersectionLibrary(id<MTLDevice> device)
    {
        IRCompiler* compiler = IRCompilerCreate();
        IRCompilerSetMinimumDeploymentTarget(compiler, IROperatingSystem_macOS, "15.0.0");
        IRCompilerSetHitgroupType(compiler, IRHitGroupTypeTriangles);
        IRMetalLibBinary* metallib = IRMetalLibBinaryCreate();
        bool success = IRMetalLibSynthesizeIndirectIntersectionFunction(compiler, metallib);
        if (!success)
        {
            ADRIA_LOG(ERROR, "Failed to synthesize indirect intersection function");
            IRMetalLibBinaryDestroy(metallib);
            IRCompilerDestroy(compiler);
            return nil;
        }
        dispatch_data_t data = IRMetalLibGetBytecodeData(metallib);
        NSError* error = nil;
        id<MTLLibrary> lib = [device newLibraryWithData:data error:&error];
        if (error || !lib)
        {
            ADRIA_LOG(ERROR, "Failed to create intersection library: %s",
                     error ? [[error localizedDescription] UTF8String] : "unknown");
        }
        IRMetalLibBinaryDestroy(metallib);
        IRCompilerDestroy(compiler);
        return lib;
    }

    MetalRayTracingPipeline::MetalRayTracingPipeline(GfxDevice* gfx, GfxRayTracingPipelineDesc const& desc)
        : raygen_pipeline(nil), intersection_table(nil), visible_function_table(nil)
    {
        @autoreleasepool
        {
            ADRIA_ASSERT(!desc.libraries.empty());
            ADRIA_ASSERT(desc.max_payload_size > 0);
            ADRIA_ASSERT(desc.max_recursion_depth > 0);

            MetalDevice* metal_gfx = static_cast<MetalDevice*>(gfx);
            id<MTLDevice> device = metal_gfx->GetMTLDevice();

            // 1. Load shader library from compiled metallib
            id<MTLLibrary> library = nil;
            NSError* error = nil;

            for (auto const& lib : desc.libraries)
            {
                if (lib.shader == nullptr) continue;

                void* shader_data = lib.shader->GetData();
                Usize shader_size = lib.shader->GetSize();

                if (shader_data && shader_size > 0)
                {
                    dispatch_data_t data = dispatch_data_create(shader_data, shader_size, dispatch_get_main_queue(), ^{});
                    library = [device newLibraryWithData:data error:&error];
                    if (error || !library)
                    {
                        ADRIA_LOG(ERROR, "Failed to create Metal library from RT shader: %s",
                                 error ? [[error localizedDescription] UTF8String] : "unknown");
                        continue;
                    }
                    break;
                }
            }

            if (!library)
            {
                ADRIA_LOG(ERROR, "Failed to create any Metal library from raytracing shaders");
                return;
            }

            id<MTLLibrary> dispatchLibrary = SynthesizeIndirectRayDispatchLibrary(device);
            if (!dispatchLibrary)
            {
                ADRIA_LOG(ERROR, "Failed to synthesize RaygenIndirection library");
                return;
            }

            NSString* dispatchKernelName = [NSString stringWithUTF8String:kRaygenIndirectionKernelName];
            id<MTLFunction> dispatchFunction = [dispatchLibrary newFunctionWithName:dispatchKernelName];
            if (!dispatchFunction)
            {
                ADRIA_LOG(ERROR, "Failed to find RaygenIndirection function in synthesized library");
                return;
            }

            id<MTLLibrary> triangleIntersectionLibrary = SynthesizeIndirectTriangleIntersectionLibrary(device);
            id<MTLFunction> triangleIntersectionFunction = nil;
            if (triangleIntersectionLibrary)
            {
                NSString* intersectionName = [NSString stringWithUTF8String:kIndirectTriangleIntersectionFunctionName];
                triangleIntersectionFunction = [triangleIntersectionLibrary newFunctionWithName:intersectionName];
                if (!triangleIntersectionFunction)
                {
                    ADRIA_LOG(WARNING, "Failed to find indirect triangle intersection function");
                }
            }

            NSMutableArray<id<MTLFunction>>* allFunctions = [NSMutableArray array];
            Uint32 next_vft_index = 1; // Reserve index 0 as null

            for (auto const& lib : desc.libraries)
            {
                if (lib.shader == nullptr) continue;
                for (auto const& export_name : lib.exports)
                {
                    NSString* functionName = [NSString stringWithUTF8String:export_name.c_str()];
                    id<MTLFunction> func = [library newFunctionWithName:functionName];
                    if (func)
                    {
                        [allFunctions addObject:func];
                        shader_to_index[export_name] = next_vft_index;
                        shader_names.insert(export_name);
                        next_vft_index++;
                    }
                }
            }

            for (auto const& hit_group : desc.hit_groups)
            {
                std::string shader_to_use;

                if (!hit_group.closest_hit_shader.empty())
                {
                    NSString* chsName = [NSString stringWithUTF8String:hit_group.closest_hit_shader.c_str()];
                    id<MTLFunction> chsFunc = [library newFunctionWithName:chsName];
                    if (chsFunc && shader_to_index.find(hit_group.closest_hit_shader) == shader_to_index.end())
                    {
                        [allFunctions addObject:chsFunc];
                        shader_to_index[hit_group.closest_hit_shader] = next_vft_index;
                        shader_names.insert(hit_group.closest_hit_shader);
                        next_vft_index++;
                    }
                    shader_to_use = hit_group.closest_hit_shader;
                }

                if (!hit_group.any_hit_shader.empty())
                {
                    NSString* ahsName = [NSString stringWithUTF8String:hit_group.any_hit_shader.c_str()];
                    id<MTLFunction> ahsFunc = [library newFunctionWithName:ahsName];
                    if (ahsFunc && shader_to_index.find(hit_group.any_hit_shader) == shader_to_index.end())
                    {
                        [allFunctions addObject:ahsFunc];
                        shader_to_index[hit_group.any_hit_shader] = next_vft_index;
                        shader_names.insert(hit_group.any_hit_shader);
                        next_vft_index++;
                    }
                    shader_to_use = hit_group.any_hit_shader;
                }

                shader_names.insert(hit_group.name);
                if (!shader_to_use.empty())
                {
                    auto it = shader_to_index.find(shader_to_use);
                    if (it != shader_to_index.end())
                    {
                        shader_to_index[hit_group.name] = it->second;
                    }
                }
            }

            if (triangleIntersectionFunction)
            {
                [allFunctions addObject:triangleIntersectionFunction];
            }

            MTLLinkedFunctions* linkedFunctions = [[MTLLinkedFunctions alloc] init];
            linkedFunctions.functions = allFunctions;

            MTLComputePipelineDescriptor* pipelineDescriptor = [[MTLComputePipelineDescriptor alloc] init];
            pipelineDescriptor.computeFunction = dispatchFunction;
            pipelineDescriptor.linkedFunctions = linkedFunctions;
            pipelineDescriptor.maxCallStackDepth = desc.max_recursion_depth;
            pipelineDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;

            error = nil;
            raygen_pipeline = [device newComputePipelineStateWithDescriptor:pipelineDescriptor
                                                                     options:MTLPipelineOptionNone
                                                                  reflection:nil
                                                                       error:&error];

            if (raygen_pipeline == nil || error != nil)
            {
                if (error != nil)
                {
                    ADRIA_LOG(ERROR, "Failed to create Metal ray tracing pipeline: %s",
                             [[error localizedDescription] UTF8String]);
                }
                return;
            }

            {
                MTLVisibleFunctionTableDescriptor* vftDesc = [[MTLVisibleFunctionTableDescriptor alloc] init];
                vftDesc.functionCount = next_vft_index;

                visible_function_table = [raygen_pipeline newVisibleFunctionTableWithDescriptor:vftDesc];

                for (id<MTLFunction> func in allFunctions)
                {
                    NSString* funcName = [func name];
                    std::string funcNameStr = [funcName UTF8String];

                    auto it = shader_to_index.find(funcNameStr);
                    if (it != shader_to_index.end())
                    {
                        id<MTLFunctionHandle> handle = [raygen_pipeline functionHandleWithFunction:func];
                        if (handle)
                        {
                            [visible_function_table setFunction:handle atIndex:it->second];
                        }
                    }
                }
            }

            if (triangleIntersectionFunction)
            {
                MTLIntersectionFunctionTableDescriptor* iftDesc = [[MTLIntersectionFunctionTableDescriptor alloc] init];
                iftDesc.functionCount = 1;

                intersection_table = [raygen_pipeline newIntersectionFunctionTableWithDescriptor:iftDesc];

                id<MTLFunctionHandle> intersectionHandle = [raygen_pipeline functionHandleWithFunction:triangleIntersectionFunction];
                if (intersectionHandle)
                {
                    [intersection_table setFunction:intersectionHandle atIndex:0];
                }
            }

            CacheShaderNames(desc);
        }
    }

    MetalRayTracingPipeline::~MetalRayTracingPipeline()
    {
        @autoreleasepool
        {
            visible_function_table = nil;
            intersection_table = nil;
            raygen_pipeline = nil;
        }
    }

    Bool MetalRayTracingPipeline::IsValid() const
    {
        return raygen_pipeline != nil;
    }

    void* MetalRayTracingPipeline::GetNative() const
    {
        return (__bridge void*)raygen_pipeline;
    }

    Bool MetalRayTracingPipeline::HasShader(Char const* name) const
    {
        ADRIA_ASSERT(name != nullptr);
        return shader_names.find(name) != shader_names.end();
    }

    Uint32 MetalRayTracingPipeline::GetShaderFunctionIndex(Char const* name) const
    {
        ADRIA_ASSERT(name != nullptr);
        auto it = shader_to_index.find(name);
        if (it != shader_to_index.end())
        {
            return it->second;
        }
        return UINT32_MAX;
    }

    void MetalRayTracingPipeline::CacheShaderNames(GfxRayTracingPipelineDesc const& desc)
    {
    }
}
