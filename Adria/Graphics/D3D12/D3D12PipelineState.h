#include "Graphics/GfxPipelineState.h"

namespace adria
{
	class D3D12Device;

	class D3D12PipelineState final : public GfxPipelineState
	{
		friend class D3D12Device;

	public:
		virtual GfxPipelineStateType GetType() const override { return type; }
		virtual void* GetNative() const override { return pso.Get(); }

	private:
		Ref<ID3D12PipelineState> pso;
		GfxPipelineStateType type;

	private:
		D3D12PipelineState(GfxDevice* gfx, GfxGraphicsPipelineStateDesc const& desc);
		D3D12PipelineState(GfxDevice* gfx, GfxComputePipelineStateDesc const& desc);
		D3D12PipelineState(GfxDevice* gfx, GfxMeshShaderPipelineStateDesc const& desc);
	};
}
