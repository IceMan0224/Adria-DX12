#import <Metal/Metal.h>
#include "MetalRayTracingShaderBindings.h"
#include "MetalRayTracingPipeline.h"
#include "MetalDevice.h"

namespace adria
{
    MetalRayTracingShaderBindings::MetalRayTracingShaderBindings(MetalRayTracingPipeline const* _pipeline)
        : pipeline(_pipeline)
        , shader_binding_table(nil)
        , miss_shader_base_index(0)
        , hit_group_base_index(0)
        , callable_shader_base_index(0)
        , is_committed(false)
    {
        ADRIA_ASSERT(pipeline != nullptr);
    }

    MetalRayTracingShaderBindings::~MetalRayTracingShaderBindings()
    {
        @autoreleasepool
        {
            shader_binding_table = nil;
        }
    }

    void MetalRayTracingShaderBindings::SetRayGenShader(Char const* name, void const* local_data, Uint32 data_size)
    {
        ADRIA_ASSERT(name != nullptr);
        ADRIA_ASSERT(pipeline->HasShader(name));
        ray_gen_record.Init(name, local_data, data_size);
        is_committed = false;
    }

    GfxShaderGroupHandle MetalRayTracingShaderBindings::AddMissShader(Char const* name, void const* local_data, Uint32 data_size)
    {
        ADRIA_ASSERT(name != nullptr);
        ADRIA_ASSERT(pipeline->HasShader(name));

        Uint32 index = static_cast<Uint32>(miss_shader_records.size());
        miss_shader_records.emplace_back();
        miss_shader_records.back().Init(name, local_data, data_size);
        is_committed = false;

        return GfxShaderGroupHandle(index);
    }

    GfxShaderGroupHandle MetalRayTracingShaderBindings::AddHitGroup(Char const* name, void const* local_data, Uint32 data_size)
    {
        ADRIA_ASSERT(name != nullptr);
        ADRIA_ASSERT(pipeline->HasShader(name));

        Uint32 index = static_cast<Uint32>(hit_group_records.size());
        hit_group_records.emplace_back();
        hit_group_records.back().Init(name, local_data, data_size);
        is_committed = false;

        return GfxShaderGroupHandle(index);
    }

    GfxShaderGroupHandle MetalRayTracingShaderBindings::AddCallableShader(Char const* name, void const* local_data, Uint32 data_size)
    {
        ADRIA_ASSERT(name != nullptr);
        ADRIA_ASSERT(pipeline->HasShader(name));

        Uint32 index = static_cast<Uint32>(callable_shader_records.size());
        callable_shader_records.emplace_back();
        callable_shader_records.back().Init(name, local_data, data_size);
        is_committed = false;

        return GfxShaderGroupHandle(index);
    }

    void MetalRayTracingShaderBindings::Commit()
    {
        // Metal's shader binding is handled differently than D3D12
        // In Metal, we use visible function tables which are set at dispatch time
        // The actual binding happens in the command encoder, so we just need to track
        // which shaders are being used
        miss_shader_base_index = 1; 
        hit_group_base_index = miss_shader_base_index + static_cast<Uint32>(miss_shader_records.size());
        callable_shader_base_index = hit_group_base_index + static_cast<Uint32>(hit_group_records.size());

        is_committed = true;
    }

    Uint32 MetalRayTracingShaderBindings::GetMissShaderIndex(GfxShaderGroupHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= miss_shader_records.size())
        {
            return UINT32_MAX;
        }
        return miss_shader_base_index + handle.index;
    }

    Uint32 MetalRayTracingShaderBindings::GetHitGroupIndex(GfxShaderGroupHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= hit_group_records.size())
        {
            return UINT32_MAX;
        }
        return hit_group_base_index + handle.index;
    }

    Uint32 MetalRayTracingShaderBindings::GetCallableShaderIndex(GfxShaderGroupHandle handle) const
    {
        if (!handle.IsValid() || handle.index >= callable_shader_records.size())
        {
            return UINT32_MAX;
        }
        return callable_shader_base_index + handle.index;
    }
}
