#pragma once
#include "Graphics/GfxRayTracingShaderBindings.h"
#include "Graphics/GfxDynamicAllocation.h"

namespace adria
{
	class D3D12RayTracingPipeline;
	class GfxLinearDynamicAllocator;
	class GfxRingDynamicAllocator;

	class D3D12RayTracingShaderBindings final : public GfxRayTracingShaderBindings
	{
	public:
		explicit D3D12RayTracingShaderBindings(D3D12RayTracingPipeline const* pipeline);
		virtual ~D3D12RayTracingShaderBindings();

		virtual void SetRayGenShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
		virtual GfxShaderGroupHandle AddMissShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
		virtual GfxShaderGroupHandle AddHitGroup(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;
		virtual GfxShaderGroupHandle AddCallableShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) override;

		virtual void Commit() override;

		virtual Uint32 GetMissShaderIndex(GfxShaderGroupHandle handle) const override;
		virtual Uint32 GetHitGroupIndex(GfxShaderGroupHandle handle) const override;
		virtual Uint32 GetCallableShaderIndex(GfxShaderGroupHandle handle) const override;

		void CommitWithAllocator(GfxLinearDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc);
		void CommitWithAllocator(GfxRingDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc);

		D3D12_DISPATCH_RAYS_DESC const& GetDispatchRaysDesc() const { return dispatch_rays_desc; }

	private:
		struct ShaderRecord
		{
			using ShaderIdentifier = Uint8[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];

			ShaderIdentifier shader_id = {};
			std::unique_ptr<Uint8[]> local_root_args = nullptr;
			Uint32 local_root_args_size = 0;

			ShaderRecord() = default;

			void Init(void const* _shader_id, void const* _local_root_args = nullptr, Uint32 _local_root_args_size = 0)
			{
				local_root_args_size = _local_root_args_size;
				memcpy(shader_id, _shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				if (local_root_args_size > 0 && _local_root_args != nullptr)
				{
					local_root_args = std::make_unique<Uint8[]>(local_root_args_size);
					memcpy(local_root_args.get(), _local_root_args, local_root_args_size);
				}
			}
		};

	private:
		D3D12RayTracingPipeline const* pipeline;
		ID3D12StateObjectProperties* state_object_properties;

		ShaderRecord ray_gen_record;
		Uint32 ray_gen_record_size;

		std::vector<ShaderRecord> miss_shader_records;
		Uint32 miss_shader_record_size;

		std::vector<ShaderRecord> hit_group_records;
		Uint32 hit_group_record_size;

		std::vector<ShaderRecord> callable_shader_records;
		Uint32 callable_shader_record_size;

		D3D12_DISPATCH_RAYS_DESC dispatch_rays_desc;
		Bool is_committed;

	private:
		template<typename AllocatorT>
		void CommitImplWithAllocator(AllocatorT& allocator, D3D12_DISPATCH_RAYS_DESC& desc);
	};
}