#pragma once
#include "UpscalerPass.h"

namespace adria
{
	class GfxDevice;
	class MLDevice;
	class GfxComputePipelineState;
	class RenderGraph;
	class PostProcessor;

	enum class TensorLayout
	{
		Default,
		NHWC
	};

	class DirectMLUpscalerPass : public UpscalerPass
	{
		static constexpr Uint32 UPSAMPLE_LAYER_COUNT = 2;
		static constexpr Uint32 CONV_LAYER_COUNT = 7;
		static constexpr Uint32 INTERMEDIATE_BUFFER_COUNT = 2;

	public:
		explicit DirectMLUpscalerPass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual void OnSceneInitialized() override;
		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual Bool IsSupported() const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 display_width;
		Uint32 display_height;
		Bool supported = false;
		std::unique_ptr<GfxComputePipelineState> tensor_to_texture_pso;
		std::unique_ptr<GfxComputePipelineState> texture_to_tensor_pso;

		Ref<IDMLDevice>              m_dmlDevice;
		Ref<IDMLCommandRecorder>     m_dmlCommandRecorder;

		TensorLayout				 m_tensorLayout;
		Ref<ID3D12Resource>          m_modelInput;
		Ref<ID3D12Resource>          m_modelOutput;
		// DirectMLX Model Resources
		Ref<ID3D12Resource>          m_modelConvFilterWeights[CONV_LAYER_COUNT];
		Ref<ID3D12Resource>          m_modelConvBiasWeights[CONV_LAYER_COUNT];

		Ref<ID3D12Resource>          m_modelPersistentResource;
		Ref<ID3D12Resource>          m_modelTemporaryResource;

		// DirectMLX operations
		Ref<IDMLCompiledOperator>    m_dmlGraph;
		Ref<IDMLBindingTable>        m_dmlBindingTable;
		Ref<IDMLOperatorInitializer> m_dmlOpInitializer;

	private:
		void CreatePSOs();
		void CreateDirectMLResources();
		void InitializeDirectMLResources();
	};
}