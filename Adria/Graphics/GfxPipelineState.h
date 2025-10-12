#pragma once
#include "GfxPipelineStateFwd.h"
#include "GfxStates.h"
#include "GfxShaderKey.h"
#include "GfxInputLayout.h"
#include "Rendering/ShaderManager.h"
#include "Utilities/Delegate.h"
#include "Utilities/Hash.h"

namespace adria
{
	class GfxDevice;

	enum class GfxRootSignatureID : Uint8
	{
		Invalid,
		Common
	};

	class GfxPipelineState
	{
	public:
		virtual ~GfxPipelineState() = default;
		virtual GfxPipelineStateType GetType() const = 0;
		virtual void* GetNative() const = 0;
	};

	struct GfxGraphicsPipelineStateDesc
	{
		GfxRasterizerState rasterizer_state{};
		GfxBlendState blend_state{};
		GfxDepthStencilState depth_state{};
		GfxPrimitiveTopologyType topology_type = GfxPrimitiveTopologyType::Triangle;
		Uint32 num_render_targets = 0;
		GfxFormat rtv_formats[8] = {};
		GfxFormat dsv_format = GfxFormat::UNKNOWN;
		GfxInputLayout input_layout;
		GfxShaderKey VS;
		GfxShaderKey PS;
		GfxShaderKey DS;
		GfxShaderKey HS;
		GfxShaderKey GS;
		Uint32 sample_mask = UINT_MAX;
		GfxRootSignatureID root_signature = GfxRootSignatureID::Common;
	};

	struct GfxComputePipelineStateDesc
	{
		GfxShaderKey CS;
		GfxRootSignatureID root_signature = GfxRootSignatureID::Common;
	};

	struct GfxMeshShaderPipelineStateDesc
	{
		GfxRasterizerState rasterizer_state{};
		GfxBlendState blend_state{};
		GfxDepthStencilState depth_state{};
		GfxPrimitiveTopologyType topology_type = GfxPrimitiveTopologyType::Triangle;
		Uint32 num_render_targets = 0;
		GfxFormat rtv_formats[8] = {};
		GfxFormat dsv_format = GfxFormat::UNKNOWN;
		GfxShaderKey AS;
		GfxShaderKey MS;
		GfxShaderKey PS;
		Uint32 sample_mask = UINT_MAX;
		GfxRootSignatureID root_signature = GfxRootSignatureID::Common;
	};


	struct GfxGraphicsPipelineStateDescHash
	{
		ADRIA_NODISCARD Uint64 operator()(GfxGraphicsPipelineStateDesc const& desc) const
		{
			HashState state;
			state.Combine(crc64(reinterpret_cast<Char const*>(&desc), sizeof(GfxGraphicsPipelineStateDesc)));
			state.Combine(desc.VS.GetHash());
			state.Combine(desc.PS.GetHash());
			state.Combine(desc.DS.GetHash());
			state.Combine(desc.HS.GetHash());
			state.Combine(desc.GS.GetHash());
			return state;
		}
	};
	struct GfxComputePipelineStateDescHash
	{
		ADRIA_NODISCARD Uint64 operator()(GfxComputePipelineStateDesc const& desc) const
		{
			HashState state;
			state.Combine(crc64(reinterpret_cast<Char const*>(&desc), sizeof(GfxComputePipelineStateDesc)));
			state.Combine(desc.CS.GetHash());
			return state;
		}
	};
	struct GfxMeshShaderPipelineStateDescHash
	{
		ADRIA_NODISCARD Uint64 operator()(GfxMeshShaderPipelineStateDesc const& desc) const
		{
			HashState state;
			state.Combine(crc64(reinterpret_cast<Char const*>(&desc), sizeof(GfxMeshShaderPipelineStateDesc)));
			state.Combine(desc.AS.GetHash());
			state.Combine(desc.MS.GetHash());
			state.Combine(desc.PS.GetHash());
			return state;
		}
	};


	template<GfxPipelineStateType>
	struct PSOTraits;

	template<>
	struct PSOTraits<GfxPipelineStateType::Graphics>
	{
		using PSODescType = GfxGraphicsPipelineStateDesc;
		using PSODescHasher = GfxGraphicsPipelineStateDescHash;
	};
	template<>
	struct PSOTraits<GfxPipelineStateType::Compute>
	{
		using PSODescType = GfxComputePipelineStateDesc;
		using PSODescHasher = GfxComputePipelineStateDescHash;
	};
	template<>
	struct PSOTraits<GfxPipelineStateType::MeshShader>
	{
		using PSODescType = GfxMeshShaderPipelineStateDesc;
		using PSODescHasher = GfxMeshShaderPipelineStateDescHash;
	};

	template<GfxPipelineStateType Type>
	class GfxManagedPipelineState
	{
		using PSODesc = typename PSOTraits<Type>::PSODescType;
		static constexpr GfxPipelineStateType PSOType = Type;

	public:
		GfxManagedPipelineState(GfxDevice* gfx, PSODesc const& desc) : gfx(gfx), desc(desc)
		{
			Create();
			event_handle = ShaderManager::GetShaderRecompiledEvent().AddMember(&GfxManagedPipelineState::OnShaderRecompiled, *this);
		}
		~GfxManagedPipelineState()
		{
			ShaderManager::GetShaderRecompiledEvent().Remove(event_handle);
		}

		ADRIA_NONCOPYABLE(GfxManagedPipelineState)
		ADRIA_DEFAULT_MOVABLE(GfxManagedPipelineState)

		GfxPipelineState const* Get() const { return pso.get(); }
		GfxPipelineState const* operator->() const { return pso.get(); }

	private:
		GfxDevice* gfx;
		PSODesc desc;
		std::unique_ptr<GfxPipelineState> pso;
		DelegateHandle event_handle;

	private:
		void OnShaderRecompiled(GfxShaderKey const& recompiled_shader)
		{
			Bool needs_recreate = false;
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				needs_recreate = (desc.VS == recompiled_shader || desc.PS == recompiled_shader ||
								  desc.DS == recompiled_shader || desc.HS == recompiled_shader ||
								  desc.GS == recompiled_shader);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				needs_recreate = (desc.CS == recompiled_shader);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				needs_recreate = (desc.AS == recompiled_shader || desc.MS == recompiled_shader ||
								  desc.PS == recompiled_shader);
			}
			if (needs_recreate)
			{
				Create();
			}
		}
		void Create()
		{
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				pso = gfx->CreateGraphicsPipelineState(desc);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				pso = gfx->CreateComputePipelineState(desc);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				pso = gfx->CreateMeshShaderPipelineState(desc);
			}
		}
	};

	using GfxGraphicsPipelineState		= GfxManagedPipelineState<GfxPipelineStateType::Graphics>;
	using GfxComputePipelineState		= GfxManagedPipelineState<GfxPipelineStateType::Compute>;
	using GfxMeshShaderPipelineState	= GfxManagedPipelineState<GfxPipelineStateType::MeshShader>;
}