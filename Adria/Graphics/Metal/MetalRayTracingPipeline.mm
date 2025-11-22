#import <Metal/Metal.h>
#include "MetalRayTracingPipeline.h"
#include "MetalDevice.h"
#include "MetalShaderReflection.h"
#include "Utilities/StringConversions.h"

namespace adria
{
    ADRIA_LOG_CHANNEL(Graphics);
    
    MetalRayTracingPipeline::MetalRayTracingPipeline(GfxDevice* gfx, GfxRayTracingPipelineDesc const& desc)
        : raygen_pipeline(nil), intersection_table(nil)
    {
        @autoreleasepool
        {
            ADRIA_ASSERT(!desc.libraries.empty());
            ADRIA_ASSERT(desc.max_payload_size > 0);
            ADRIA_ASSERT(desc.max_recursion_depth > 0);

            MetalDevice* metal_gfx = static_cast<MetalDevice*>(gfx);
            id<MTLDevice> device = metal_gfx->GetMTLDevice();

            id<MTLLibrary> library = nil;
            NSError* error = nil;

            ADRIA_LOG(INFO, "Ray tracing pipeline has %zu libraries", desc.libraries.size());

            for (auto const& lib : desc.libraries)
            {
                if (lib.shader == nullptr)
                {
                    ADRIA_LOG(WARNING, "Skipping null shader in library");
                    continue;
                }

                void* shader_data = lib.shader->GetData();
                Usize shader_size = lib.shader->GetSize();
                GfxShaderDesc const& shader_desc = lib.shader->GetDesc();
                ADRIA_LOG(INFO, "Processing library shader: file=%s, entry=%s, data=%p, size=%llu",
                         shader_desc.file.c_str(), shader_desc.entry_point.c_str(), shader_data, shader_size);

                if (shader_data && shader_size > 0)
                {
                    // Shader data is pure metallib (no header to skip)
                    ADRIA_LOG(INFO, "Creating dispatch_data_t...");
                    // Use DISPATCH_DATA_DESTRUCTOR_FREE with NULL to avoid freeing memory we don't own
                    dispatch_data_t data = dispatch_data_create(shader_data, shader_size, dispatch_get_main_queue(), ^{
                        // Empty destructor - we don't own this memory
                    });
                    ADRIA_LOG(INFO, "dispatch_data_t created: %p", data);

                    ADRIA_LOG(INFO, "Creating Metal library from data...");
                    library = [device newLibraryWithData:data error:&error];
                    ADRIA_LOG(INFO, "Metal library creation returned: library=%p, error=%p", library, error);

                    if (error || !library)
                    {
                        if (error)
                        {
                            ADRIA_LOG(ERROR, "Failed to create Metal library from raytracing shader: %s",
                                     [[error localizedDescription] UTF8String]);
                        }
                        else
                        {
                            ADRIA_LOG(ERROR, "Failed to create Metal library: library is nil but no error");
                        }
                        continue;
                    }

                    NSArray<NSString*>* functionNames = [library functionNames];
                    ADRIA_LOG(INFO, "Successfully created Metal library with %lu functions", [functionNames count]);
                    ADRIA_LOG(INFO, "functionNames pointer: %p", functionNames);
                    if (functionNames)
                    {
                        for (NSString* name in functionNames)
                        {
                            ADRIA_LOG(INFO, "  - Function: %s", [name UTF8String]);
                        }
                    }
                    else
                    {
                        ADRIA_LOG(ERROR, "functionNames is nil!");
                    }
                    break;
                }
                else
                {
                    ADRIA_LOG(ERROR, "Shader data is null or size is 0");
                }
            }

            if (!library)
            {
                ADRIA_LOG(ERROR, "Failed to create any Metal library from raytracing shaders");
                return;
            }

            NSArray<NSString*>* functionNames = [library functionNames];

            // Log available functions in the library for debugging
            ADRIA_LOG(INFO, "Metal library contains %lu functions:", [functionNames count]);
            for (NSString* name in functionNames)
            {
                ADRIA_LOG(INFO, "  Available function: %s", [name UTF8String]);
            }

            id<MTLFunction> raygenFunction = nil;
            for (auto const& lib : desc.libraries)
            {
                if (lib.shader == nullptr)
                {
                    continue;
                }

                for (auto const& export_name : lib.exports)
                {
                    if (raygenFunction == nil)
                    {
                        ADRIA_LOG(INFO, "Searching for export: %s", export_name.c_str());
                        NSString* functionName = [NSString stringWithUTF8String:export_name.c_str()];
                        raygenFunction = [library newFunctionWithName:functionName];
                        if (raygenFunction)
                        {
                            ADRIA_LOG(INFO, "Successfully found function: %s", export_name.c_str());
                            shader_names.insert(export_name);
                            break;
                        }
                    }
                }
            }

            if (raygenFunction == nil)
            {
                ADRIA_LOG(ERROR, "Failed to find ray generation function in Metal library");
                return;
            }

            MTLLinkedFunctions* linkedFunctions = nil;

            if (!desc.hit_groups.empty() || desc.libraries.size() > 1)
            {
                linkedFunctions = [[MTLLinkedFunctions alloc] init];
                NSMutableArray<id<MTLFunction>>* functions = [NSMutableArray array];

                for (auto const& hit_group : desc.hit_groups)
                {
                    if (!hit_group.closest_hit_shader.empty())
                    {
                        NSString* chsName = [NSString stringWithUTF8String:hit_group.closest_hit_shader.c_str()];
                        id<MTLFunction> chsFunc = [library newFunctionWithName:chsName];
                        if (chsFunc)
                        {
                            [functions addObject:chsFunc];
                            shader_names.insert(hit_group.closest_hit_shader);
                        }
                    }

                    if (!hit_group.any_hit_shader.empty())
                    {
                        NSString* ahsName = [NSString stringWithUTF8String:hit_group.any_hit_shader.c_str()];
                        id<MTLFunction> ahsFunc = [library newFunctionWithName:ahsName];
                        if (ahsFunc)
                        {
                            [functions addObject:ahsFunc];
                            shader_names.insert(hit_group.any_hit_shader);
                        }
                    }

                    if (!hit_group.intersection_shader.empty())
                    {
                        NSString* isName = [NSString stringWithUTF8String:hit_group.intersection_shader.c_str()];
                        id<MTLFunction> isFunc = [library newFunctionWithName:isName];
                        if (isFunc)
                        {
                            [functions addObject:isFunc];
                            shader_names.insert(hit_group.intersection_shader);
                        }
                    }

                    shader_names.insert(hit_group.name);
                }

                for (auto const& lib : desc.libraries)
                {
                    for (auto const& export_name : lib.exports)
                    {
                        if (shader_names.find(export_name) != shader_names.end())
                        {
                            continue;
                        }

                        NSString* functionName = [NSString stringWithUTF8String:export_name.c_str()];
                        id<MTLFunction> func = [library newFunctionWithName:functionName];
                        if (func)
                        {
                            [functions addObject:func];
                            shader_names.insert(export_name);
                        }
                    }
                }
                linkedFunctions.functions = functions;
            }

            MTLComputePipelineDescriptor* pipelineDescriptor = [[MTLComputePipelineDescriptor alloc] init];
            pipelineDescriptor.computeFunction = raygenFunction;
            pipelineDescriptor.linkedFunctions = linkedFunctions;
            pipelineDescriptor.maxCallStackDepth = desc.max_recursion_depth;
            pipelineDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;

            error = nil;
            raygen_pipeline = [device newComputePipelineStateWithDescriptor:pipelineDescriptor
                                                                     options:MTLPipelineOptionArgumentInfo | MTLPipelineOptionBufferTypeInfo
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

            Bool has_intersection_shaders = false;
            for (auto const& hit_group : desc.hit_groups)
            {
                if (!hit_group.intersection_shader.empty())
                {
                    has_intersection_shaders = true;
                    break;
                }
            }

            if (has_intersection_shaders)
            {
                MTLIntersectionFunctionTableDescriptor* intersectionTableDesc = [[MTLIntersectionFunctionTableDescriptor alloc] init];
                intersectionTableDesc.functionCount = desc.hit_groups.size();

                intersection_table = [raygen_pipeline newIntersectionFunctionTableWithDescriptor:intersectionTableDesc];

                Uint32 index = 0;
                for (auto const& hit_group : desc.hit_groups)
                {
                    if (!hit_group.intersection_shader.empty())
                    {
                        NSString* isName = [NSString stringWithUTF8String:hit_group.intersection_shader.c_str()];
                        id<MTLFunction> isFunc = [library newFunctionWithName:isName];
                        if (isFunc)
                        {
                            id<MTLFunctionHandle> handle = [raygen_pipeline functionHandleWithFunction:isFunc];
                            [intersection_table setFunction:handle atIndex:index];
                        }
                    }
                    index++;
                }
            }

            CacheShaderNames(desc);
        }
    }

    MetalRayTracingPipeline::~MetalRayTracingPipeline()
    {
        @autoreleasepool
        {
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

    void MetalRayTracingPipeline::CacheShaderNames(GfxRayTracingPipelineDesc const& desc)
    {
    }
}
