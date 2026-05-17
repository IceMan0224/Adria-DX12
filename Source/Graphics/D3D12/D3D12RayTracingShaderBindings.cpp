#include "D3D12RayTracingShaderBindings.h"
#include "D3D12RayTracingPipeline.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDynamicAllocator.h"
#include "Utilities/StringConversions.h"

namespace adria
{
	D3D12RayTracingShaderBindings::D3D12RayTracingShaderBindings(D3D12RayTracingPipeline const* _pipeline)
		: pipeline(_pipeline)
		, state_object_properties(nullptr)
		, ray_gen_record()
		, ray_gen_record_size(0)
		, miss_shader_records()
		, miss_shader_record_size(0)
		, hit_group_records()
		, hit_group_record_size(0)
		, callable_shader_records()
		, callable_shader_record_size(0)
		, dispatch_rays_desc{}
		, is_committed(false)
	{
		ADRIA_ASSERT(pipeline != nullptr);
		ADRIA_ASSERT(pipeline->IsValid());
		state_object_properties = pipeline->GetD3D12Properties();
		ADRIA_ASSERT(state_object_properties != nullptr);
	}

	D3D12RayTracingShaderBindings::~D3D12RayTracingShaderBindings()
	{
		state_object_properties = nullptr;
		pipeline = nullptr;
	}

	void D3D12RayTracingShaderBindings::SetRayGenShader(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(!is_committed);

		std::wstring wide_name = ToWideString(name);
		void const* shader_id = state_object_properties->GetShaderIdentifier(wide_name.c_str());
		ADRIA_ASSERT(shader_id != nullptr && "Ray generation shader not found in pipeline");

		ray_gen_record.Init(shader_id, local_data, data_size);
		ray_gen_record_size = static_cast<Uint32>(AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
	}

	GfxShaderGroupHandle D3D12RayTracingShaderBindings::AddMissShader(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(!is_committed);

		std::wstring wide_name = ToWideString(name);
		void const* shader_id = state_object_properties->GetShaderIdentifier(wide_name.c_str());
		ADRIA_ASSERT(shader_id != nullptr && "Miss shader not found in pipeline");

		Uint32 index = static_cast<Uint32>(miss_shader_records.size());
		miss_shader_records.emplace_back();
		miss_shader_records[index].Init(shader_id, local_data, data_size);

		Uint32 record_size = static_cast<Uint32>(AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
		miss_shader_record_size = std::max(miss_shader_record_size, record_size);

		return GfxShaderGroupHandle(index);
	}

	GfxShaderGroupHandle D3D12RayTracingShaderBindings::AddHitGroup(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(!is_committed);

		std::wstring wide_name = ToWideString(name);
		void const* shader_id = state_object_properties->GetShaderIdentifier(wide_name.c_str());
		ADRIA_ASSERT(shader_id != nullptr && "Hit group not found in pipeline");

		Uint32 index = static_cast<Uint32>(hit_group_records.size());
		hit_group_records.emplace_back();
		hit_group_records[index].Init(shader_id, local_data, data_size);

		Uint32 record_size = static_cast<Uint32>(AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
		hit_group_record_size = std::max(hit_group_record_size, record_size);

		return GfxShaderGroupHandle(index);
	}

	GfxShaderGroupHandle D3D12RayTracingShaderBindings::AddCallableShader(Char const* name, void const* local_data, Uint32 data_size)
	{
		ADRIA_ASSERT(name != nullptr);
		ADRIA_ASSERT(!is_committed);

		std::wstring wide_name = ToWideString(name);
		void const* shader_id = state_object_properties->GetShaderIdentifier(wide_name.c_str());
		ADRIA_ASSERT(shader_id != nullptr && "Callable shader not found in pipeline");

		Uint32 index = static_cast<Uint32>(callable_shader_records.size());
		callable_shader_records.emplace_back();
		callable_shader_records[index].Init(shader_id, local_data, data_size);

		Uint32 record_size = static_cast<Uint32>(AlignUp(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
		callable_shader_record_size = std::max(callable_shader_record_size, record_size);

		return GfxShaderGroupHandle(index);
	}

	void D3D12RayTracingShaderBindings::Commit()
	{
		ADRIA_ASSERT(!is_committed);
		ADRIA_ASSERT(ray_gen_record_size > 0 && "Ray generation shader must be set before committing");
		is_committed = true;
	}

	Uint32 D3D12RayTracingShaderBindings::GetMissShaderIndex(GfxShaderGroupHandle handle) const
	{
		ADRIA_ASSERT(handle.IsValid());
		ADRIA_ASSERT(handle.index < miss_shader_records.size());
		return handle.index;
	}

	Uint32 D3D12RayTracingShaderBindings::GetHitGroupIndex(GfxShaderGroupHandle handle) const
	{
		ADRIA_ASSERT(handle.IsValid());
		ADRIA_ASSERT(handle.index < hit_group_records.size());
		return handle.index;
	}

	Uint32 D3D12RayTracingShaderBindings::GetCallableShaderIndex(GfxShaderGroupHandle handle) const
	{
		ADRIA_ASSERT(handle.IsValid());
		ADRIA_ASSERT(handle.index < callable_shader_records.size());
		return handle.index;
	}

	void D3D12RayTracingShaderBindings::CommitWithAllocator(GfxLinearDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
	{
		CommitImplWithAllocator(allocator, desc);
	}

	void D3D12RayTracingShaderBindings::CommitWithAllocator(GfxRingDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
	{
		CommitImplWithAllocator(allocator, desc);
	}

	template<typename AllocatorT>
	void D3D12RayTracingShaderBindings::CommitImplWithAllocator(AllocatorT& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
	{
		ADRIA_ASSERT(is_committed);
		ADRIA_ASSERT(ray_gen_record_size > 0);

		// Calculate section sizes
		Uint32 ray_gen_section = ray_gen_record_size;
		Uint32 ray_gen_section_aligned = static_cast<Uint32>(AlignUp(ray_gen_section, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

		Uint32 miss_section = miss_shader_record_size * static_cast<Uint32>(miss_shader_records.size());
		Uint32 miss_section_aligned = static_cast<Uint32>(AlignUp(miss_section, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

		Uint32 hit_section = hit_group_record_size * static_cast<Uint32>(hit_group_records.size());
		Uint32 hit_section_aligned = static_cast<Uint32>(AlignUp(hit_section, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

		Uint32 callable_section = callable_shader_record_size * static_cast<Uint32>(callable_shader_records.size());
		Uint32 callable_section_aligned = static_cast<Uint32>(AlignUp(callable_section, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

		Uint32 total_size = static_cast<Uint32>(AlignUp(ray_gen_section_aligned + miss_section_aligned + hit_section_aligned + callable_section_aligned, 256));

		// Allocate shader table memory
		GfxDynamicAllocation allocation = allocator.Allocate(total_size, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
		ADRIA_ASSERT(allocation.cpu_address != nullptr);
		ADRIA_ASSERT(allocation.gpu_address != 0);

		Uint8* p_start = static_cast<Uint8*>(allocation.cpu_address);
		Uint8* p_data = p_start;

		// Write ray generation shader record
		memcpy(p_data, ray_gen_record.shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		if (ray_gen_record.local_root_args_size > 0 && ray_gen_record.local_root_args != nullptr)
		{
			memcpy(p_data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, ray_gen_record.local_root_args.get(), ray_gen_record.local_root_args_size);
		}
		p_data += ray_gen_record_size;
		p_data = p_start + ray_gen_section_aligned;

		// Write miss shader records
		for (Uint64 i = 0; i < miss_shader_records.size(); ++i)
		{
			ShaderRecord const& record = miss_shader_records[i];
			memcpy(p_data, record.shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			if (record.local_root_args_size > 0 && record.local_root_args != nullptr)
			{
				memcpy(p_data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, record.local_root_args.get(), record.local_root_args_size);
			}
			p_data += miss_shader_record_size;
		}
		p_data = p_start + ray_gen_section_aligned + miss_section_aligned;

		// Write hit group records
		for (Uint64 i = 0; i < hit_group_records.size(); ++i)
		{
			ShaderRecord const& record = hit_group_records[i];
			memcpy(p_data, record.shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			if (record.local_root_args_size > 0 && record.local_root_args != nullptr)
			{
				memcpy(p_data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, record.local_root_args.get(), record.local_root_args_size);
			}
			p_data += hit_group_record_size;
		}
		p_data = p_start + ray_gen_section_aligned + miss_section_aligned + hit_section_aligned;

		// Write callable shader records
		for (Uint64 i = 0; i < callable_shader_records.size(); ++i)
		{
			ShaderRecord const& record = callable_shader_records[i];
			memcpy(p_data, record.shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			if (record.local_root_args_size > 0 && record.local_root_args != nullptr)
			{
				memcpy(p_data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, record.local_root_args.get(), record.local_root_args_size);
			}
			p_data += callable_shader_record_size;
		}

		// Fill dispatch rays descriptor
		desc.RayGenerationShaderRecord.StartAddress = allocation.gpu_address;
		desc.RayGenerationShaderRecord.SizeInBytes = ray_gen_section;

		desc.MissShaderTable.StartAddress = allocation.gpu_address + ray_gen_section_aligned;
		desc.MissShaderTable.SizeInBytes = miss_section;
		desc.MissShaderTable.StrideInBytes = miss_shader_record_size;

		desc.HitGroupTable.StartAddress = allocation.gpu_address + ray_gen_section_aligned + miss_section_aligned;
		desc.HitGroupTable.SizeInBytes = hit_section;
		desc.HitGroupTable.StrideInBytes = hit_group_record_size;

		desc.CallableShaderTable.StartAddress = allocation.gpu_address + ray_gen_section_aligned + miss_section_aligned + hit_section_aligned;
		desc.CallableShaderTable.SizeInBytes = callable_section;
		desc.CallableShaderTable.StrideInBytes = callable_shader_record_size;

		dispatch_rays_desc = desc;
	}
}