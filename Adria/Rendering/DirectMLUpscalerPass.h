#pragma once
#include "UpscalerPass.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class GfxComputePipelineState;
	template<Bool>
	class GfxRingDescriptorAllocator;
	using GfxOnlineDescriptorAllocator = GfxRingDescriptorAllocator<GFX_MULTITHREADED>;
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
		Bool needs_init = true;

		TensorLayout				 tensor_layout;
		std::unique_ptr<GfxComputePipelineState> tensor_to_texture_pso;
		std::unique_ptr<GfxComputePipelineState> texture_to_tensor_pso;

		Ref<IDMLDevice>              dml_device;
		Ref<IDMLCommandRecorder>     dml_command_recorder;

		std::unique_ptr<GfxBuffer>   model_input;
		std::unique_ptr<GfxBuffer>   model_output;
		std::unique_ptr<GfxBuffer>   model_conv_filter_weights[CONV_LAYER_COUNT];
		std::unique_ptr<GfxBuffer>   model_conv_bias_weights[CONV_LAYER_COUNT];

		std::unique_ptr<GfxBuffer>   model_persistent_resource;
		std::unique_ptr<GfxBuffer>   model_temporary_resource;

		std::unique_ptr<GfxOnlineDescriptorAllocator> dml_heap;
		Ref<IDMLCompiledOperator>    dml_graph;
		Ref<IDMLBindingTable>        dml_binding_table;
		Ref<IDMLOperatorInitializer> dml_op_initializer;

		Bool dml_managed_weights = true;

	private:
		void CreatePSOs();

		void AddTextureToTensorPass(RenderGraph&, PostProcessor const*);
		void AddUpscalingPass(RenderGraph&);
		void AddTensorToTexturePass(RenderGraph&, PostProcessor*);

		void CreateDirectMLResources();
		void InitializeDirectMLResources();

		std::unique_ptr<GfxBuffer> CreateFilterTensor(std::vector<Float> const&, std::vector<Float> const&, std::span<const Uint32>);
		std::unique_ptr<GfxBuffer> CreateBiasTensor(std::vector<Float> const&, std::span<const Uint32>);
		std::vector<Uint16> CompressFilterWeights(std::vector<Float> const&, std::vector<Float> const&, std::span<const Uint32>);
		std::vector<Uint16> CompressBiasWeights(std::vector<Float> const&, std::span<const Uint32>);
	};
}