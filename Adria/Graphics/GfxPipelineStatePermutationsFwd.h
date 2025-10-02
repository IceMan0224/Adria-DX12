#pragma once

namespace adria
{
	class GfxManagedGraphicsPipelineState;
	class GfxManagedComputePipelineState;
	class GfxManagedMeshShaderPipelineState;

	enum class GfxPipelineStateType : Uint8;

	template<GfxPipelineStateType Type>
	class GfxPipelineStatePermutations;

	using GfxGraphicsPipelineStatePermutations = GfxPipelineStatePermutations<GfxPipelineStateType::Graphics>;
	using GfxComputePipelineStatePermutations = GfxPipelineStatePermutations<GfxPipelineStateType::Compute>;
	using GfxMeshShaderPipelineStatePermutations = GfxPipelineStatePermutations<GfxPipelineStateType::MeshShader>;
}

