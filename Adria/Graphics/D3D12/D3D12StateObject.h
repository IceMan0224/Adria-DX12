#pragma once
#include "Graphics/GfxDefines.h"

namespace adria
{
	class GfxDevice;

	enum class D3D12StateObjectType
	{
		Collection,
		RayTracingPipeline,
		Executable
	};

	class D3D12StateObjectBuilder;
	class D3D12StateObject
	{
		friend class D3D12StateObjectBuilder;
		friend class GfxCommandList;
		friend class GfxRayTracingShaderTable;

	public:
		Bool IsValid() const { return d3d12_so != nullptr; }
		ID3D12StateObject* GetNative() const { return d3d12_so.Get(); }

	private:
		Ref<ID3D12StateObject> d3d12_so;

	private:
		explicit D3D12StateObject(ID3D12StateObject* so)
		{
			d3d12_so.Attach(so);
		}
	};

	class D3D12StateObjectBuilder
	{
		static constexpr Uint64 MAX_SUBOBJECT_DESC_SIZE = sizeof(D3D12_HIT_GROUP_DESC);
	public:
		explicit D3D12StateObjectBuilder(Uint64 max_subobjects)
			: max_subobjects(max_subobjects)
			, num_subobjects(0u)
			, subobjects(max_subobjects)
			, subobject_data(max_subobjects* MAX_SUBOBJECT_DESC_SIZE)
		{}

		template<typename SubObjectDesc>
		void AddSubObject(SubObjectDesc const& desc)
		{
			if constexpr (std::is_same_v<SubObjectDesc, D3D12_STATE_OBJECT_CONFIG>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_GLOBAL_ROOT_SIGNATURE>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_LOCAL_ROOT_SIGNATURE>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_NODE_MASK>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_DXIL_LIBRARY_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_EXISTING_COLLECTION_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_RAYTRACING_SHADER_CONFIG>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_RAYTRACING_PIPELINE_CONFIG>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_HIT_GROUP_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_WORK_GRAPH_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH);
			else
				return nullptr;
		}

		D3D12StateObject* CreateStateObject(D3D12Device* gfx, D3D12StateObjectType type = D3D12StateObjectType::RayTracingPipeline)
		{
			D3D12_STATE_OBJECT_TYPE d3d12_type;
			switch (type)
			{
			case D3D12StateObjectType::RayTracingPipeline: d3d12_type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE; break;
			case D3D12StateObjectType::Collection:		 d3d12_type = D3D12_STATE_OBJECT_TYPE_COLLECTION; break;
			case D3D12StateObjectType::Executable:		 d3d12_type = D3D12_STATE_OBJECT_TYPE_EXECUTABLE; break;
			default: d3d12_type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
			}

			auto BuildDescription = [this](D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc)
				{
					desc.Type = type;
					desc.NumSubobjects = static_cast<Uint32>(num_subobjects);
					desc.pSubobjects = num_subobjects ? subobjects.data() : nullptr;
				};
			D3D12_STATE_OBJECT_DESC desc{};
			BuildDescription(d3d12_type, desc);
			ID3D12StateObject* state_obj = nullptr;
			ID3D12Device5* d3d12gfx = gfx->GetD3D12Device();
			HRESULT hr = d3d12gfx->CreateStateObject(&desc, IID_PPV_ARGS(&state_obj));
			GFX_CHECK_HR(hr);
			return new D3D12StateObject(state_obj);
		}

		Uint64 GetNumSubobjects() const
		{
			return num_subobjects;
		}

		D3D12_STATE_SUBOBJECT const* GetSubobject(Uint32 index) const
		{
			ADRIA_ASSERT(index < num_subobjects);
			return &subobjects[index];
		}

	private:
		std::vector<Uint8> subobject_data;
		std::vector<D3D12_STATE_SUBOBJECT> subobjects;
		Uint64 const max_subobjects;
		Uint64 num_subobjects;

	private:
		void AddSubObject(void const* desc, Uint64 desc_size, D3D12_STATE_SUBOBJECT_TYPE type)
		{
			ADRIA_ASSERT(desc != nullptr);
			ADRIA_ASSERT(desc_size > 0);
			ADRIA_ASSERT(type < D3D12_STATE_SUBOBJECT_TYPE_MAX_VALID);
			ADRIA_ASSERT(desc_size <= MAX_SUBOBJECT_DESC_SIZE);
			ADRIA_ASSERT(num_subobjects < max_subobjects);

			const Uint64 subobject_offset = num_subobjects * MAX_SUBOBJECT_DESC_SIZE;
			memcpy(subobject_data.data() + subobject_offset, desc, desc_size);

			D3D12_STATE_SUBOBJECT& subobject = subobjects[num_subobjects];
			subobject.Type = type;
			subobject.pDesc = subobject_data.data() + subobject_offset;
			++num_subobjects;
		}
	};
}