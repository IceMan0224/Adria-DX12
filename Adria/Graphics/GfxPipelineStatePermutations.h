#pragma once
#include "GfxPipelineState.h"
#include "GfxShader.h"

namespace adria
{
	template<GfxPipelineStateType Type>
	class GfxPipelineStatePermutations
	{
		static constexpr GfxPipelineStateType PSOType = Type;
		using PSODesc = typename PSOTraits<Type>::PSODescType;
		using PSODescHasher = typename PSOTraits<Type>::PSODescHasher;

		struct PSOCacheEntry
		{
			std::unique_ptr<GfxPipelineState> pso;
			PSODesc desc;
		};
		using PSOPermutationMap = std::unordered_map<Uint64, PSOCacheEntry>;
		using ShaderDependencyMap = std::unordered_map<GfxShaderKey, std::vector<Uint64>>;

	public:
		GfxPipelineStatePermutations(GfxDevice* gfx, PSODesc const& desc) : gfx(gfx), base_pso_desc(desc), current_pso_desc(desc)
		{
			event_handle = ShaderManager::GetShaderRecompiledEvent().AddMember(&GfxPipelineStatePermutations::OnShaderRecompiled, *this);
		}
		ADRIA_NONCOPYABLE(GfxPipelineStatePermutations)
		~GfxPipelineStatePermutations()
		{
			ShaderManager::GetShaderRecompiledEvent().Remove(event_handle);
		}

		void AddDefine(Char const* name, Char const* value)
		{
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				current_pso_desc.VS.AddDefine(name, value);
				current_pso_desc.PS.AddDefine(name, value);
				current_pso_desc.DS.AddDefine(name, value);
				current_pso_desc.HS.AddDefine(name, value);
				current_pso_desc.GS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				current_pso_desc.CS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				current_pso_desc.MS.AddDefine(name, value);
				current_pso_desc.AS.AddDefine(name, value);
				current_pso_desc.PS.AddDefine(name, value);
			}
		}
		void AddDefine(Char const* name)
		{
			AddDefine(name, "");
		}
		template<GfxShaderStage stage>
		void AddDefine(Char const* name, Char const* value)
		{
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				if (stage == GfxShaderStage::VS) current_pso_desc.VS.AddDefine(name, value);
				if (stage == GfxShaderStage::PS) current_pso_desc.PS.AddDefine(name, value);
				if (stage == GfxShaderStage::DS) current_pso_desc.DS.AddDefine(name, value);
				if (stage == GfxShaderStage::HS) current_pso_desc.HS.AddDefine(name, value);
				if (stage == GfxShaderStage::GS) current_pso_desc.GS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				if (stage == GfxShaderStage::CS) current_pso_desc.CS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				if (stage == GfxShaderStage::MS) current_pso_desc.MS.AddDefine(name, value);
				if (stage == GfxShaderStage::AS) current_pso_desc.AS.AddDefine(name, value);
				if (stage == GfxShaderStage::PS) current_pso_desc.PS.AddDefine(name, value);
			}
		}
		template<GfxShaderStage stage>
		void AddDefine(Char const* name)
		{
			AddDefine<stage>(name, "");
		}

		void SetCullMode(GfxCullMode cull_mode) 
		{
			if constexpr (Type != GfxPipelineStateType::Compute)
			{
				current_pso_desc.rasterizer_state.cull_mode = cull_mode;
			}
		}
		void SetFillMode(GfxFillMode fill_mode) 
		{
			if constexpr (Type != GfxPipelineStateType::Compute)
			{
				current_pso_desc.rasterizer_state.fill_mode = fill_mode;
			}
		}
		void SetTopologyType(GfxPrimitiveTopologyType topology_type) 
		{
			if constexpr (Type != GfxPipelineStateType::Compute)
			{
				current_pso_desc.topology_type = topology_type;
			}
		}

		template<typename F> requires std::is_invocable_v<F, PSODesc&>
		void ModifyDesc(F&& f)
		{
			f(current_pso_desc);
		}

		GfxPipelineState const* Get() const
		{
			Uint64 const pso_hash = PSODescHasher{}(current_pso_desc);
			auto it = pso_permutations.find(pso_hash);

			if (it == pso_permutations.end())
			{
				std::unique_ptr<GfxPipelineState> new_pso;
				if constexpr (PSOType == GfxPipelineStateType::Graphics)
				{
					new_pso = gfx->CreateGraphicsPipelineState(current_pso_desc);
				}
				else if constexpr (PSOType == GfxPipelineStateType::Compute)
				{
					new_pso = gfx->CreateComputePipelineState(current_pso_desc);
				}
				else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
				{
					new_pso = gfx->CreateMeshShaderPipelineState(current_pso_desc);
				}

				RegisterDependencies(current_pso_desc, pso_hash);

				PSOCacheEntry cache_entry{ .pso = std::move(new_pso), .desc = current_pso_desc };
				it = pso_permutations.emplace(pso_hash, std::move(cache_entry)).first;
			}

			GfxPipelineState const* pso = it->second.pso.get();
			current_pso_desc = base_pso_desc;
			return pso;
		}

	private:
		GfxDevice* gfx;
		PSODesc const base_pso_desc;
		mutable PSODesc current_pso_desc;

		mutable PSOPermutationMap pso_permutations;
		mutable ShaderDependencyMap shader_dependencies;
		DelegateHandle event_handle;

	private:
		void OnShaderRecompiled(GfxShaderKey const& recompiled_shader);
		void RegisterDependencies(PSODesc const& desc, Uint64 pso_hash) const
		{
			auto RegisterSingleDependency = [&](GfxShaderKey const& key)
				{
					if (key.IsValid())
					{
						shader_dependencies[key].push_back(pso_hash);
					}
				};

			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				RegisterSingleDependency(desc.VS);
				RegisterSingleDependency(desc.PS);
				RegisterSingleDependency(desc.DS);
				RegisterSingleDependency(desc.HS);
				RegisterSingleDependency(desc.GS);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				RegisterSingleDependency(desc.CS);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				RegisterSingleDependency(desc.AS);
				RegisterSingleDependency(desc.MS);
				RegisterSingleDependency(desc.PS);
			}
		}
	};
}