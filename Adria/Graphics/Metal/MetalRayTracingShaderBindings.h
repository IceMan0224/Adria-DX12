#pragma once
#include "Graphics/GfxRayTracingShaderBindings.h"
#import <Metal/Metal.h>
#include <vector>
#include <memory>

@protocol MTLBuffer;

namespace adria
{
    class MetalRayTracingPipeline;

    class MetalRayTracingShaderBindings : public GfxRayTracingShaderBindings
    {
    public:
        explicit MetalRayTracingShaderBindings(MetalRayTracingPipeline const* pipeline);
        virtual ~MetalRayTracingShaderBindings() override;

        virtual void SetRayGenShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
        virtual GfxShaderGroupHandle AddMissShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
        virtual GfxShaderGroupHandle AddHitGroup(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
        virtual GfxShaderGroupHandle AddCallableShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;

        virtual void Commit() override;

        virtual Uint32 GetMissShaderIndex(GfxShaderGroupHandle handle) const override;
        virtual Uint32 GetHitGroupIndex(GfxShaderGroupHandle handle) const override;
        virtual Uint32 GetCallableShaderIndex(GfxShaderGroupHandle handle) const override;

        id<MTLBuffer> GetShaderBindingTable() const { return shader_binding_table; }
        Uint32 GetMissShaderBaseIndex() const { return miss_shader_base_index; }
        Uint32 GetHitGroupBaseIndex() const { return hit_group_base_index; }
        Uint32 GetCallableShaderBaseIndex() const { return callable_shader_base_index; }

    private:
        struct ShaderRecord
        {
            std::string name;
            std::unique_ptr<Uint8[]> local_data = nullptr;
            Uint32 local_data_size = 0;

            ShaderRecord() = default;

            void Init(Char const* shader_name, void const* _local_data = nullptr, Uint32 _local_data_size = 0)
            {
                name = shader_name;
                local_data_size = _local_data_size;
                if (local_data_size > 0 && _local_data != nullptr)
                {
                    local_data = std::make_unique<Uint8[]>(local_data_size);
                    memcpy(local_data.get(), _local_data, local_data_size);
                }
            }
        };

    private:
        MetalRayTracingPipeline const* pipeline;

        ShaderRecord ray_gen_record;
        std::vector<ShaderRecord> miss_shader_records;
        std::vector<ShaderRecord> hit_group_records;
        std::vector<ShaderRecord> callable_shader_records;

        id<MTLBuffer> shader_binding_table;
        Uint32 miss_shader_base_index;
        Uint32 hit_group_base_index;
        Uint32 callable_shader_base_index;

        Bool is_committed;
    };
}
