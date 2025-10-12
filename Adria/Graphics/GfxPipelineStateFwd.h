#pragma once


namespace adria
{
	enum class GfxPipelineStateType : Uint8
	{
		Graphics,
		Compute,
		MeshShader
	};

	template<GfxPipelineStateType Type>
	class GfxManagedPipelineState;

	using GfxGraphicsPipelineState = GfxManagedPipelineState<GfxPipelineStateType::Graphics>;
	using GfxComputePipelineState = GfxManagedPipelineState<GfxPipelineStateType::Compute>;
	using GfxMeshShaderPipelineState = GfxManagedPipelineState<GfxPipelineStateType::MeshShader>;

	template<GfxPipelineStateType Type>
	class GfxPipelineStatePermutations;

	using GfxGraphicsPipelineStatePermutations = GfxPipelineStatePermutations<GfxPipelineStateType::Graphics>;
	using GfxComputePipelineStatePermutations = GfxPipelineStatePermutations<GfxPipelineStateType::Compute>;
	using GfxMeshShaderPipelineStatePermutations = GfxPipelineStatePermutations<GfxPipelineStateType::MeshShader>;
}