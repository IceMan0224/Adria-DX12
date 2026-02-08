#import <Metal/Metal.h>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>
#include "MetalRayTracingShaderBindings.h"
#include "MetalRayTracingPipeline.h"
#include "MetalDevice.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Utilities/Align.h"

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

    MetalShaderTableDescriptors MetalRayTracingShaderBindings::CommitAndGetShaderTables(GfxLinearDynamicAllocator& allocator)
    {
        ADRIA_ASSERT(is_committed);
        ADRIA_ASSERT(!ray_gen_record.name.empty() && "Ray generation shader must be set");

        constexpr Uint32 SHADER_IDENTIFIER_SIZE = sizeof(IRShaderIdentifier);
        constexpr Uint32 SHADER_RECORD_ALIGNMENT = 64;
        constexpr Uint32 SHADER_TABLE_ALIGNMENT = 256;

        Uint32 ray_gen_record_size = SHADER_IDENTIFIER_SIZE + ray_gen_record.local_data_size;
        ray_gen_record_size = static_cast<Uint32>(AlignUp(ray_gen_record_size, SHADER_RECORD_ALIGNMENT));

        Uint32 miss_shader_record_size = SHADER_IDENTIFIER_SIZE;
        for (auto const& record : miss_shader_records)
        {
            Uint32 record_size = SHADER_IDENTIFIER_SIZE + record.local_data_size;
            miss_shader_record_size = std::max(miss_shader_record_size, record_size);
        }
        miss_shader_record_size = static_cast<Uint32>(AlignUp(miss_shader_record_size, SHADER_RECORD_ALIGNMENT));

        Uint32 hit_group_record_size = SHADER_IDENTIFIER_SIZE;
        for (auto const& record : hit_group_records)
        {
            Uint32 record_size = SHADER_IDENTIFIER_SIZE + record.local_data_size;
            hit_group_record_size = std::max(hit_group_record_size, record_size);
        }
        hit_group_record_size = static_cast<Uint32>(AlignUp(hit_group_record_size, SHADER_RECORD_ALIGNMENT));

        Uint32 callable_shader_record_size = SHADER_IDENTIFIER_SIZE;
        for (auto const& record : callable_shader_records)
        {
            Uint32 record_size = SHADER_IDENTIFIER_SIZE + record.local_data_size;
            callable_shader_record_size = std::max(callable_shader_record_size, record_size);
        }
        callable_shader_record_size = static_cast<Uint32>(AlignUp(callable_shader_record_size, SHADER_RECORD_ALIGNMENT));

        Uint32 ray_gen_section_aligned = static_cast<Uint32>(AlignUp(ray_gen_record_size, SHADER_TABLE_ALIGNMENT));
        Uint32 miss_section = miss_shader_record_size * static_cast<Uint32>(miss_shader_records.size());
        Uint32 miss_section_aligned = static_cast<Uint32>(AlignUp(miss_section, SHADER_TABLE_ALIGNMENT));
        Uint32 hit_section = hit_group_record_size * static_cast<Uint32>(hit_group_records.size());
        Uint32 hit_section_aligned = static_cast<Uint32>(AlignUp(hit_section, SHADER_TABLE_ALIGNMENT));
        Uint32 callable_section = callable_shader_record_size * static_cast<Uint32>(callable_shader_records.size());
        Uint32 callable_section_aligned = static_cast<Uint32>(AlignUp(callable_section, SHADER_TABLE_ALIGNMENT));

        Uint32 total_size = static_cast<Uint32>(AlignUp(
            ray_gen_section_aligned + miss_section_aligned + hit_section_aligned + callable_section_aligned,
            SHADER_TABLE_ALIGNMENT
        ));

        GfxDynamicAllocation allocation = allocator.Allocate(total_size, SHADER_TABLE_ALIGNMENT);
        ADRIA_ASSERT(allocation.cpu_address != nullptr);

        Uint8* p_start = static_cast<Uint8*>(allocation.cpu_address);
        Uint64 gpu_start = allocation.gpu_address;
        Uint8* p_data = p_start;
        MetalShaderTableDescriptors descriptors{};

        IRShaderIdentifier raygen_identifier{};
        raygen_identifier.shaderHandle = 0;
        memcpy(p_data, &raygen_identifier, SHADER_IDENTIFIER_SIZE);
        if (ray_gen_record.local_data_size > 0 && ray_gen_record.local_data != nullptr)
        {
            memcpy(p_data + SHADER_IDENTIFIER_SIZE, ray_gen_record.local_data.get(), ray_gen_record.local_data_size);
        }
        descriptors.ray_gen_record_gpu_addr = gpu_start + static_cast<Uint64>(p_data - p_start);
        p_data = p_start + ray_gen_section_aligned;

        if (!miss_shader_records.empty())
        {
            descriptors.miss_shader_table_gpu_addr = gpu_start + static_cast<Uint64>(p_data - p_start);
            descriptors.miss_shader_stride = miss_shader_record_size;
            descriptors.miss_shader_size = miss_section;

            for (auto const& record : miss_shader_records)
            {
                Uint32 miss_index = pipeline->GetShaderFunctionIndex(record.name.c_str());
                ADRIA_ASSERT(miss_index != UINT32_MAX && "Miss shader not found in pipeline");
                IRShaderIdentifier identifier{};
                identifier.shaderHandle = static_cast<Uint64>(miss_index);
                memcpy(p_data, &identifier, SHADER_IDENTIFIER_SIZE);
                if (record.local_data_size > 0 && record.local_data != nullptr)
                {
                    memcpy(p_data + SHADER_IDENTIFIER_SIZE, record.local_data.get(), record.local_data_size);
                }
                p_data += miss_shader_record_size;
            }
        }
        p_data = p_start + ray_gen_section_aligned + miss_section_aligned;

        if (!hit_group_records.empty())
        {
            descriptors.hit_group_table_gpu_addr = gpu_start + static_cast<Uint64>(p_data - p_start);
            descriptors.hit_group_stride = hit_group_record_size;
            descriptors.hit_group_size = hit_section;

            for (auto const& record : hit_group_records)
            {
                Uint32 hit_index = pipeline->GetShaderFunctionIndex(record.name.c_str());
                ADRIA_ASSERT(hit_index != UINT32_MAX && "Hit group not found in pipeline");
                IRShaderIdentifier identifier{};
                identifier.shaderHandle = static_cast<Uint64>(hit_index);
                memcpy(p_data, &identifier, SHADER_IDENTIFIER_SIZE);
                if (record.local_data_size > 0 && record.local_data != nullptr)
                {
                    memcpy(p_data + SHADER_IDENTIFIER_SIZE, record.local_data.get(), record.local_data_size);
                }
                p_data += hit_group_record_size;
            }
        }
        p_data = p_start + ray_gen_section_aligned + miss_section_aligned + hit_section_aligned;

        if (!callable_shader_records.empty())
        {
            descriptors.callable_shader_table_gpu_addr = gpu_start + static_cast<Uint64>(p_data - p_start);
            descriptors.callable_shader_stride = callable_shader_record_size;
            descriptors.callable_shader_size = callable_section;

            for (auto const& record : callable_shader_records)
            {
                Uint32 callable_index = pipeline->GetShaderFunctionIndex(record.name.c_str());
                ADRIA_ASSERT(callable_index != UINT32_MAX && "Callable shader not found in pipeline");
                IRShaderIdentifier identifier{};
                identifier.shaderHandle = static_cast<Uint64>(callable_index);
                memcpy(p_data, &identifier, SHADER_IDENTIFIER_SIZE);
                if (record.local_data_size > 0 && record.local_data != nullptr)
                {
                    memcpy(p_data + SHADER_IDENTIFIER_SIZE, record.local_data.get(), record.local_data_size);
                }
                p_data += callable_shader_record_size;
            }
        }

        return descriptors;
    }
}
