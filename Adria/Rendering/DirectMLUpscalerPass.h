#pragma once
#include "UpscalerPass.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
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
		Uint32 render_width;
		Uint32 render_height;
		Bool supported = false;

		std::unique_ptr<GfxComputePipelineState> tensor_to_texture_pso;
		std::unique_ptr<GfxComputePipelineState> texture_to_tensor_pso;
		TensorLayout				 tensor_layout;

		Ref<IDMLDevice>              dml_device;
		Ref<IDMLCommandRecorder>     dml_command_recorder;

		std::unique_ptr<GfxBuffer>   model_input;
		std::unique_ptr<GfxBuffer>   model_output;
		std::unique_ptr<GfxBuffer>   model_conv_filter_weights[CONV_LAYER_COUNT];
		std::unique_ptr<GfxBuffer>   model_conv_bias_weights[CONV_LAYER_COUNT];

		std::unique_ptr<GfxBuffer>   model_persistent_resource;
		std::unique_ptr<GfxBuffer>   model_temporary_resource;

		Ref<IDMLCompiledOperator>    dml_graph;
		Ref<IDMLBindingTable>        dml_binding_table;
		Ref<IDMLOperatorInitializer> dml_op_initializer;

		Bool dml_managed_weights = false;

	private:
		void CreatePSOs();
		void CreateDirectMLResources();
		void InitializeDirectMLResources();

		void AddImageToTensorPass(RenderGraph&);
		void AddUpscalingPass(RenderGraph&);
		void AddTensorToImagePass(RenderGraph&);
	};
}