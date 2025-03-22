#include "DirectMLUpscalerPass.h"
#include "ShaderManager.h"
#include "BlackboardData.h"
#include "Postprocessor.h"
#include "Core/Paths.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"
#include "Utilities/FloatCompressor.h"

#pragma comment(lib, "DirectML.lib")

namespace adria
{
	namespace util
	{
		constexpr Uint32 BUFFER_LENGTH = 256;
		Bool LoadWeights(std::string const& weights_file_name, std::unordered_map<std::string, std::vector<Float>>& weight_map)
		{
			std::ifstream input(weights_file_name, std::ifstream::binary);
			if (!(input) || !(input.good()) || !(input.is_open()))
			{
				ADRIA_ERROR("[DirectML] Unable to open weight file: %s", weights_file_name.c_str());
				return false;
			}

			Int32 weight_tensor_count;
			try
			{
				input.read(reinterpret_cast<char*>(&weight_tensor_count), 4);
			}
			catch (const std::ifstream::failure&)
			{
				ADRIA_ERROR("[DirectML] Invalid weight file: %s", weights_file_name.c_str());
				return false;
			}
			if (weight_tensor_count < 0)
			{
				ADRIA_ERROR("[DirectML] Invalid weight file: %s", weights_file_name.c_str());
				return false;
			}

			Uint32 name_length;
			Uint32 w_length;
			Char name_buf[BUFFER_LENGTH];
			try
			{
				while (weight_tensor_count--)
				{
					input.read(reinterpret_cast<Char*>(&name_length), sizeof(name_length));
					if (name_length > BUFFER_LENGTH - 1)
					{
						ADRIA_ERROR("[DirectML] name_len exceeds BUFFER_LENGTH");
						return false;
					}
					input.read(name_buf, name_length);
					name_buf[name_length] = '\0';
					std::string name(name_buf);

					input.read(reinterpret_cast<Char*>(&w_length), sizeof(Uint32));
					weight_map[name] = std::vector<Float>(w_length);
					input.read(reinterpret_cast<Char*>(weight_map[name].data()), sizeof(Float) * w_length);
					ADRIA_DEBUG("[DirectML] Loaded Tensor: %s -> %d", name.c_str(), w_length);
				}
				input.close();
			}
			catch (std::ifstream::failure const&)
			{
				ADRIA_ERROR("[DirectML] Invalid tensor data");
				return false;
			}
			catch (std::out_of_range const&)
			{
				ADRIA_ERROR("[DirectML] Invalid tensor format");
				return false;
			}

			return true;
		}
		void GetStrides(Uint32 const* sizes, TensorLayout layout, Uint32* strides)
		{
			switch (layout)
			{
			case TensorLayout::NHWC:
				strides[0] = sizes[1] * sizes[2] * sizes[3];
				strides[1] = 1;
				strides[2] = sizes[1] * sizes[3];
				strides[3] = sizes[1];
				break;
			default:
				strides[0] = sizes[1] * sizes[2] * sizes[3];
				strides[1] = sizes[2] * sizes[3];
				strides[2] = sizes[3];
				strides[3] = 1;
			}
		}
		std::pair<std::vector<Uint16>, Uint64> CompressWeights(TensorLayout layout, std::vector<Float> const& filter_weights, std::vector<Float> const& scale_weights, std::vector<Float> const& shift_weights, std::span<const Uint32> sizes, Bool is_filter)
		{
			std::vector<Uint16> result;
			Uint32 N = sizes[0], C = sizes[1], H = sizes[2], W = sizes[3];
			Uint64 total_size = N * C * H * W;
			Bool const use_scale_shift = !scale_weights.empty();
			if (is_filter)
			{
				result.reserve(total_size);
				for (Uint32 n = 0; n < N; ++n)
				{
					if (layout == TensorLayout::Default)
					{
						for (Uint32 i = 0; i < C * H * W; ++i)
						{
							Uint32 idx = n * C * H * W + i;
							Float scaled_weight = use_scale_shift ? filter_weights[idx] * scale_weights[n] : filter_weights[idx];
							result.push_back(FloatCompressor::Compress(scaled_weight));
						}
					}
					else
					{
						for (Uint32 h = 0; h < H; h++)
						{
							for (Uint32 w = 0; w < W; w++)
							{
								for (Uint32 c = 0; c < C; c++)
								{
									Uint32 idx = w + h * W + c * H * W + n * C * H * W;
									Float scaled_weight = use_scale_shift ? filter_weights[idx] * scale_weights[n] : filter_weights[idx];
									result.push_back(FloatCompressor::Compress(scaled_weight));
								}
							}
						}
					}
				}
			}
			else  // bias
			{
				result.reserve(N);
				for (Uint32 n = 0; n < N; ++n)
				{
					result.push_back(FloatCompressor::Compress(shift_weights[n]));
				}
				total_size = N;
			}
			return { std::move(result), total_size * sizeof(Uint16) };
		}

		void CreateWeightTensors(GfxDevice* gfx, TensorLayout layout, 
			std::vector<Float> const& conv_layer_weights, std::vector<Float> const& scale_layer_weights, 
			std::vector<Float> const& shift_layer_weights, std::span<Uint32 const> filter_sizes, 
			std::unique_ptr<GfxBuffer>& filter_tensor, std::unique_ptr<GfxBuffer>& bias_tensor)
		{
			Bool const use_scale_shift = !scale_layer_weights.empty();
			auto [filter_data, filter_size] = CompressWeights(layout, conv_layer_weights,
				use_scale_shift ? scale_layer_weights : std::vector<Float>{},
				use_scale_shift ? shift_layer_weights : std::vector<Float>{},
				filter_sizes, true);

			Uint32 filter_strides[4];
			GetStrides(filter_sizes.data(), layout, filter_strides);
			Uint64 filter_buffer_size = DMLCalcBufferTensorSize(DML_TENSOR_DATA_TYPE_FLOAT16, 4, filter_sizes.data(), filter_strides);

			GfxBufferDesc filter_tensor_desc{};
			filter_tensor_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
			filter_tensor_desc.size = filter_buffer_size;
			filter_tensor_desc.resource_usage = GfxResourceUsage::Default;
			GfxBufferData gfx_filter_data(filter_data.data());

			filter_tensor = gfx->CreateBuffer(filter_tensor_desc, gfx_filter_data);
			if (use_scale_shift)
			{
				Uint32 bias_sizes[] = { 1, filter_sizes[0], 1, 1 };
				auto [bias_data, bias_size] = CompressWeights(layout, {}, {}, shift_layer_weights, filter_sizes, false);

				Uint32 bias_strides[4];
				GetStrides(bias_sizes, layout, bias_strides);
				Uint64 bias_buffer_size = DMLCalcBufferTensorSize(DML_TENSOR_DATA_TYPE_FLOAT16, 4, bias_sizes, bias_strides);

				GfxBufferDesc bias_tensor_desc{};
				bias_tensor_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
				bias_tensor_desc.size = bias_buffer_size;
				bias_tensor_desc.resource_usage = GfxResourceUsage::Default;
				GfxBufferData gfx_bias_data(bias_data.data());

				bias_tensor = gfx->CreateBuffer(bias_tensor_desc, gfx_bias_data);
			}
		}
	}

	static TAutoConsoleVariable<Bool> DirectML("r.DirectML", true, "Enable or Disable DirectML Upscaler");

	DirectMLUpscalerPass::DirectMLUpscalerPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), display_width(0), display_height(0), render_width(0), render_height(0)
	{
		ADRIA_TODO("Add cmd line option for debug dml device");
		Bool const DEBUG_DML_DEVICE = true;
		GFX_CHECK_HR(DMLCreateDevice(gfx->GetDevice(), DEBUG_DML_DEVICE ? DML_CREATE_DEVICE_FLAG_DEBUG : DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(dml_device.GetAddressOf())));
	
		tensor_layout = gfx->GetVendor() == GfxVendor::Nvidia ? TensorLayout::NHWC : TensorLayout::Default;

		DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT fp16_query{ DML_TENSOR_DATA_TYPE_FLOAT16 };
		DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT fp16_supported{};
		GFX_CHECK_HR(dml_device->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT, sizeof(fp16_query), &fp16_query, sizeof(fp16_supported), &fp16_supported));
		if (!fp16_supported.IsSupported)
		{
			return;
		}
		supported = true;
		GFX_CHECK_HR(dml_device->CreateCommandRecorder(IID_PPV_ARGS(dml_command_recorder.GetAddressOf())));
		CreatePSOs();
		OnResize(w, h);
	}

	void DirectMLUpscalerPass::OnResize(Uint32 w, Uint32 h)
	{
		display_width = w, display_height = h;
		render_width = static_cast<Uint32>(display_width * 0.5f);
		render_height = static_cast<Uint32>(display_height * 0.5f);
		BroadcastRenderResolutionChanged(render_width, render_height);
	}

	void DirectMLUpscalerPass::OnSceneInitialized()
	{
		CreateDirectMLResources();
		InitializeDirectMLResources();
	}

	void DirectMLUpscalerPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		if (!IsSupported())
		{
			ADRIA_ASSERT_MSG(false, "DirectMLUpscaler is not supported on this device");
			return;
		}
		RG_SCOPE(rg, "DML Upscaling");

		rg.ImportBuffer(RG_NAME(ModelInput), model_input.get());
		rg.ImportBuffer(RG_NAME(ModelOutput), model_output.get());
		AddTextureToTensorPass(rg, postprocessor);
		AddUpscalingPass(rg);
		AddTensorToTexturePass(rg, postprocessor);
	}

	Bool DirectMLUpscalerPass::IsEnabled(PostProcessor const*) const
	{
		return DirectML.Get();
	}

	Bool DirectMLUpscalerPass::IsSupported() const
	{
		return supported;
	}

	void DirectMLUpscalerPass::GUI()
	{
		QueueGUI([&]()
			{
				if (ImGui::TreeNodeEx("DirectML Upscaler", ImGuiTreeNodeFlags_None))
				{
					ImGui::Checkbox("Enable", DirectML.GetPtr());
					ImGui::TreePop();
				}
			}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Upscaler);
	}

	void DirectMLUpscalerPass::CreatePSOs()
	{
		GfxComputePipelineStateDesc pso_desc{};
		pso_desc.CS = CS_TensorToTexture;
		tensor_to_texture_pso = gfx->CreateComputePipelineState(pso_desc);

		pso_desc.CS = CS_TextureToTensor;
		texture_to_tensor_pso = gfx->CreateComputePipelineState(pso_desc);
	}

	void DirectMLUpscalerPass::CreateDirectMLResources()
	{
		std::unordered_map<std::string, std::vector<Float>> weights;
		if (!util::LoadWeights(paths::MLDir + "weights.bin", weights))
		{
			ADRIA_ASSERT_MSG(false, "[DIRECT ML] Weight loading failed!");
		}

		Uint32 filter_sizes1[4] = { 32, 3, 5, 5 };
		util::CreateWeightTensors(gfx, tensor_layout, weights["conv1/weights"], weights["conv1/BatchNorm/scale"], weights["conv1/BatchNorm/shift"],
								  filter_sizes1, model_conv_filter_weights[0], model_conv_bias_weights[0]);

		Uint32 filter_sizes2[4] = { 64, 32, 3, 3 };
		util::CreateWeightTensors(gfx, tensor_layout, weights["conv2/weights"], weights["conv2/BatchNorm/scale"], weights["conv2/BatchNorm/shift"],
								  filter_sizes2, model_conv_filter_weights[1], model_conv_bias_weights[1]);

		Uint32 filter_sizes3[4] = { 64, 64, 3, 3 };
		util::CreateWeightTensors(gfx, tensor_layout, weights["conv3/weights"], weights["conv3/BatchNorm/scale"], weights["conv3/BatchNorm/shift"],
								  filter_sizes3, model_conv_filter_weights[2], model_conv_bias_weights[2]);

		Uint32 filter_sizes4[4] = { 32, 64, 5, 5 };
		util::CreateWeightTensors(gfx, tensor_layout, weights["conv_up1/conv/weights"], weights["conv_up1/conv/BatchNorm/scale"], weights["conv_up1/conv/BatchNorm/shift"],
								  filter_sizes4, model_conv_filter_weights[3], model_conv_bias_weights[3]);

		Uint32 filter_sizes5[4] = { 32, 32, 3, 3 };
		util::CreateWeightTensors(gfx, tensor_layout, weights["conv4/weights"], weights["conv4/BatchNorm/scale"], weights["conv4/BatchNorm/shift"],
								  filter_sizes5, model_conv_filter_weights[4], model_conv_bias_weights[4]);

		Uint32 filter_sizes6[4] = { 32, 32, 3, 3 };
		util::CreateWeightTensors(gfx, tensor_layout, weights["conv5/weights"], weights["conv5/BatchNorm/scale"], weights["conv5/BatchNorm/shift"],
								  filter_sizes6, model_conv_filter_weights[5], model_conv_bias_weights[5]);

		Uint32 filter_sizes7[4] = { 3, 32, 3, 3 };
		util::CreateWeightTensors(gfx, tensor_layout, weights["conv6/weights"], {}, {}, filter_sizes7, model_conv_filter_weights[6], model_conv_bias_weights[6]);

		// Construct a DML graph of operators

		DML_TENSOR_DATA_TYPE data_type = DML_TENSOR_DATA_TYPE_FLOAT16;
		DML_TENSOR_FLAGS flags = dml_managed_weights ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
		
		dml::TensorPolicy policy = tensor_layout == TensorLayout::Default ? dml::TensorPolicy::Default() : dml::TensorPolicy::InterleavedChannel();
		dml::Graph graph(dml_device.Get(), policy);

		using Dimensions = dml::TensorDesc::Dimensions;
		Dimensions model_input_sizes = { 1, 3, render_height, render_width };
		auto input_model = dml::InputTensor(graph, 0, dml::TensorDesc(data_type, model_input_sizes, policy));

		// conv1
		dml::Expression conv1_filter = dml::InputTensor(graph, 1, dml::TensorDesc(data_type, flags, { 32,  3, 5, 5 }, policy));
		dml::Expression conv1_bias = dml::InputTensor(graph, 2, dml::TensorDesc(data_type, flags, { 1, 32, 1, 1 }, policy));
		dml::Expression conv1 = dml::ConvolutionBuilder(input_model, conv1_filter, conv1_bias)
					    .StartPadding(std::array<Uint32, 2>{ 2u, 2u })
					    .EndPadding(std::array<Uint32, 2>{ 2u, 2u })
					    .FusedActivation(dml::FusedActivation::Relu())
					    .Build();

		// conv2
		dml::Expression conv2_filter = dml::InputTensor(graph, 3, dml::TensorDesc(data_type, flags, { 64, 32, 3, 3 }, policy));
		dml::Expression conv2_bias = dml::InputTensor(graph, 4, dml::TensorDesc(data_type, flags, { 1, 64, 1, 1 }, policy));
		dml::Expression conv2 = dml::ConvolutionBuilder(conv1, conv2_filter, conv2_bias)
						.StartPadding(std::array<Uint32, 2>{ 1u, 1u })
						.EndPadding(std::array<Uint32, 2>{ 1u, 1u })
						.FusedActivation(dml::FusedActivation::Relu())
						.Build();

		// conv3
		dml::Expression conv3_filter = dml::InputTensor(graph, 5, dml::TensorDesc(data_type, flags, { 64, 64, 3, 3 }, policy));
		dml::Expression conv3_bias = dml::InputTensor(graph, 6, dml::TensorDesc(data_type, flags, { 1, 64, 1, 1 }, policy));
		dml::Expression conv3 = dml::ConvolutionBuilder(conv2, conv3_filter, conv3_bias)
						.StartPadding(std::array<Uint32, 2>{ 1u, 1u })
						.EndPadding(std::array<Uint32, 2>{ 1u, 1u })
						.FusedActivation(dml::FusedActivation::Relu())
						.Build();

		// up1 (2x nearest-neighbor upsample)
		dml::Expression up1 = dml::Upsample2D(conv3, DML_SIZE_2D{ 2, 2 }, DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR);

		// conv_up1
		dml::Expression conv_up1_filter = dml::InputTensor(graph, 7, dml::TensorDesc(data_type, flags, { 32, 64, 5, 5 }, policy));
		dml::Expression conv_up1_bias = dml::InputTensor(graph, 8, dml::TensorDesc(data_type, flags, { 1, 32, 1, 1 }, policy));
		dml::Expression conv_up1 = dml::ConvolutionBuilder(up1, conv_up1_filter, conv_up1_bias)
						.StartPadding(std::array<Uint32, 2>{ 2u, 2u })
						.EndPadding(std::array<Uint32, 2>{ 2u, 2u })
						.FusedActivation(dml::FusedActivation::Relu())
						.Build();

		// conv4
		dml::Expression conv4_filter = dml::InputTensor(graph, 9, dml::TensorDesc(data_type, flags, { 32, 32, 3, 3 }, policy));
		dml::Expression conv4_bias = dml::InputTensor(graph, 10, dml::TensorDesc(data_type, flags, { 1, 32, 1, 1 }, policy));
		dml::Expression conv4 = dml::ConvolutionBuilder(conv_up1, conv4_filter, conv4_bias)
			.StartPadding(std::array<Uint32, 2>{ 1u, 1u })
			.EndPadding(std::array<Uint32, 2>{ 1u, 1u })
			.FusedActivation(dml::FusedActivation::Relu())
			.Build();

		// conv5
		dml::Expression conv5_filter = dml::InputTensor(graph, 11, dml::TensorDesc(data_type, flags, { 32, 32, 3, 3 }, policy));
		dml::Expression conv5_bias = dml::InputTensor(graph, 12, dml::TensorDesc(data_type, flags, { 1, 32, 1, 1 }, policy));
		dml::Expression conv5 = dml::ConvolutionBuilder(conv4, conv5_filter, conv5_bias)
						.StartPadding(std::array<Uint32, 2>{ 1u, 1u })
						.EndPadding(std::array<Uint32, 2>{ 1u, 1u })
						.FusedActivation(dml::FusedActivation::Relu())
						.Build();

		// conv6 (no bias or activation)
		dml::Expression conv6_filter = dml::InputTensor(graph, 13, dml::TensorDesc(data_type, flags, { 3, 32, 3, 3 }, policy));
		dml::Expression conv6 = dml::ConvolutionBuilder(conv5, conv6_filter)
						.StartPadding(std::array<Uint32, 2>{ 1u, 1u })
						.EndPadding(std::array<Uint32, 2>{ 1u, 1u })
						.Build();

		// Add the output of the convolutions to an upscaled version of the original image
		dml::Expression up2 = dml::Upsample2D(input_model, DML_SIZE_2D{ 2, 2 }, DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
		dml::Expression output_model = up2 + conv6;

		Uint64 model_input_buffer_size = input_model.GetOutputDesc().totalTensorSizeInBytes;
		Uint64 model_output_buffer_size = output_model.GetOutputDesc().totalTensorSizeInBytes;

		// Compile the graph
		DML_EXECUTION_FLAGS execution_flags = DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION_COMPUTATION;
		auto compiled_graph = graph.Compile(execution_flags, std::array<dml::Expression, 1>{ output_model });
		dml_graph.Attach(compiled_graph.Detach());

		// Resource for input tensor
		GfxBufferDesc model_input_buffer_desc{};
		model_input_buffer_desc.resource_usage = GfxResourceUsage::Default;
		model_input_buffer_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
		model_input_buffer_desc.size = model_input_buffer_size;
		model_input_buffer_desc.format = GfxFormat::R16_FLOAT;
		model_input = gfx->CreateBuffer(model_input_buffer_desc);

		GfxBufferDesc model_output_buffer_desc = model_input_buffer_desc;
		model_output_buffer_desc.size = model_output_buffer_size;
		model_output = gfx->CreateBuffer(model_output_buffer_desc);
	}

	void DirectMLUpscalerPass::InitializeDirectMLResources()
	{
		GfxCommandList* cmd_list = gfx->GetGraphicsCommandList();
		GFX_CHECK_HR(dml_device->CreateOperatorInitializer(1, dml_graph.GetAddressOf(), IID_PPV_ARGS(dml_op_initializer.ReleaseAndGetAddressOf())));

		DML_BINDING_PROPERTIES init_binding_props = dml_op_initializer->GetBindingProperties();
		DML_BINDING_PROPERTIES execute_binding_props = dml_graph->GetBindingProperties();

		dml_heap = std::make_unique<GfxOnlineDescriptorAllocator>(gfx, std::max(init_binding_props.RequiredDescriptorCount, execute_binding_props.RequiredDescriptorCount), 0);
		cmd_list->SetHeap(dml_heap.get());

		// Create any persistent resources required for the operators.
		if (execute_binding_props.PersistentResourceSize > 0)
		{
			GfxBufferDesc model_persistent_buffer_desc{};
			model_persistent_buffer_desc.resource_usage = GfxResourceUsage::Default;
			model_persistent_buffer_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
			model_persistent_buffer_desc.size = execute_binding_props.PersistentResourceSize;
			model_persistent_resource = gfx->CreateBuffer(model_persistent_buffer_desc);
		}

		// Temporary resource for execution
		if (execute_binding_props.TemporaryResourceSize > 0)
		{
			GfxBufferDesc model_temporary_buffer_desc{};
			model_temporary_buffer_desc.resource_usage = GfxResourceUsage::Default;
			model_temporary_buffer_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
			model_temporary_buffer_desc.size = execute_binding_props.TemporaryResourceSize;
			model_temporary_resource = gfx->CreateBuffer(model_temporary_buffer_desc);
		}

		// If the execute temporary resource isn't big enough for initialization, create a bigger buffer
		std::unique_ptr<GfxBuffer> init_temporary_resource = nullptr;
		if (init_binding_props.TemporaryResourceSize > 0)
		{
			GfxBufferDesc model_temporary_buffer_desc{};
			model_temporary_buffer_desc.resource_usage = GfxResourceUsage::Default;
			model_temporary_buffer_desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
			model_temporary_buffer_desc.size = init_binding_props.TemporaryResourceSize;
			init_temporary_resource = gfx->CreateBuffer(model_temporary_buffer_desc);
		}

		Microsoft::WRL::ComPtr<IDMLBindingTable> init_binding_table;
		ADRIA_ASSERT(init_binding_props.PersistentResourceSize == 0);

		DML_BINDING_TABLE_DESC table_desc =
		{
			dml_op_initializer.Get(),
			dml_heap->GetHandle(0),
			dml_heap->GetHandle(0),
			init_binding_props.RequiredDescriptorCount
		};
		GFX_CHECK_HR(dml_device->CreateBindingTable(&table_desc, IID_PPV_ARGS(&init_binding_table)));

		DML_BUFFER_BINDING buffer_bindings[] =
		{
			{}, // model input
			{ model_conv_filter_weights[0]->GetNative(), 0, model_conv_filter_weights[0]->GetSize() }, { model_conv_bias_weights[0]->GetNative(), 0, model_conv_bias_weights[0]->GetSize() },
			{ model_conv_filter_weights[1]->GetNative(), 0, model_conv_filter_weights[1]->GetSize() }, { model_conv_bias_weights[1]->GetNative(), 0, model_conv_bias_weights[1]->GetSize() },
			{ model_conv_filter_weights[2]->GetNative(), 0, model_conv_filter_weights[2]->GetSize() }, { model_conv_bias_weights[2]->GetNative(), 0, model_conv_bias_weights[2]->GetSize() },
			{ model_conv_filter_weights[3]->GetNative(), 0, model_conv_filter_weights[3]->GetSize() }, { model_conv_bias_weights[3]->GetNative(), 0, model_conv_bias_weights[3]->GetSize() },
			{ model_conv_filter_weights[4]->GetNative(), 0, model_conv_filter_weights[4]->GetSize() }, { model_conv_bias_weights[4]->GetNative(), 0, model_conv_bias_weights[4]->GetSize() },
			{ model_conv_filter_weights[5]->GetNative(), 0, model_conv_filter_weights[5]->GetSize() }, { model_conv_bias_weights[5]->GetNative(), 0, model_conv_bias_weights[5]->GetSize() },
			{ model_conv_filter_weights[6]->GetNative(), 0, model_conv_filter_weights[6]->GetSize() }, 
		};
		// Bind inputs for initialization, which is only necessary if we're using OWNED_BY_DML
		if (dml_managed_weights)
		{
			DML_BUFFER_ARRAY_BINDING init_input_binding = { ARRAYSIZE(buffer_bindings), buffer_bindings };
			DML_BINDING_DESC binding_desc{ DML_BINDING_TYPE_BUFFER_ARRAY, &init_input_binding };
			init_binding_table->BindInputs(1, &binding_desc);
		}
		else
		{
			init_binding_table->BindInputs(0, nullptr);
		}

		if (init_temporary_resource)
		{
			DML_BUFFER_BINDING binding = { init_temporary_resource->GetNative(), 0, init_temporary_resource->GetSize() };
			DML_BINDING_DESC binding_desc{ DML_BINDING_TYPE_BUFFER, &binding };
			init_binding_table->BindTemporaryResource(&binding_desc);
		}

		// If the operator requires a persistent resource, it must be bound as output for the initializer.
		if (model_persistent_resource)
		{
			DML_BUFFER_BINDING binding = { model_persistent_resource->GetNative(), 0, model_persistent_resource->GetSize() };
			DML_BINDING_DESC binding_desc{ DML_BINDING_TYPE_BUFFER, &binding };
			init_binding_table->BindOutputs(1, &binding_desc);
		}

		dml_command_recorder->RecordDispatch(cmd_list->GetNative(), dml_op_initializer.Get(), init_binding_table.Get());

		cmd_list->End();
		cmd_list->Submit();
		gfx->WaitForGPU();
		cmd_list->Begin();

		if (dml_managed_weights)
		{
			for (Uint32 i = 0; i < CONV_LAYER_COUNT; i++)
			{
				model_conv_filter_weights[i].reset();
				model_conv_bias_weights[i].reset();
			}
		}
		
		table_desc.Dispatchable = dml_graph.Get();
		table_desc.SizeInDescriptors = execute_binding_props.RequiredDescriptorCount;
		GFX_CHECK_HR(dml_device->CreateBindingTable(&table_desc, IID_PPV_ARGS(dml_binding_table.GetAddressOf())));

		if (model_persistent_resource)
		{
			DML_BUFFER_BINDING binding = { model_persistent_resource->GetNative(), 0, model_persistent_resource->GetSize() };
			DML_BINDING_DESC binding_desc{ DML_BINDING_TYPE_BUFFER, &binding };
			dml_binding_table->BindPersistentResource(&binding_desc);
		}

		if (model_temporary_resource)
		{
			DML_BUFFER_BINDING binding = { model_temporary_resource->GetNative(), 0, model_temporary_resource->GetSize() };
			DML_BINDING_DESC binding_desc{ DML_BINDING_TYPE_BUFFER, &binding };
			dml_binding_table->BindTemporaryResource(&binding_desc);
		}

		// Bind model inputs and outputs
		buffer_bindings[0] = DML_BUFFER_BINDING{ model_input->GetNative() };
		if (dml_managed_weights)
		{
			// Bind only the model input
			DML_BINDING_DESC input_bindings[] =
			{
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[0] },
				{ DML_BINDING_TYPE_NONE, nullptr }, { DML_BINDING_TYPE_NONE, nullptr },
				{ DML_BINDING_TYPE_NONE, nullptr }, { DML_BINDING_TYPE_NONE, nullptr },
				{ DML_BINDING_TYPE_NONE, nullptr }, { DML_BINDING_TYPE_NONE, nullptr },
				{ DML_BINDING_TYPE_NONE, nullptr }, { DML_BINDING_TYPE_NONE, nullptr },
				{ DML_BINDING_TYPE_NONE, nullptr }, { DML_BINDING_TYPE_NONE, nullptr },
				{ DML_BINDING_TYPE_NONE, nullptr }, { DML_BINDING_TYPE_NONE, nullptr },
				{ DML_BINDING_TYPE_NONE, nullptr }, // last layer has no bias
			};
			dml_binding_table->BindInputs(ARRAYSIZE(input_bindings), input_bindings);
		}
		else
		{
			// Bind everything
			DML_BINDING_DESC input_bindings[] =
			{
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[0] }, // model input
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[1] }, { DML_BINDING_TYPE_BUFFER, &buffer_bindings[2] },
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[3] }, { DML_BINDING_TYPE_BUFFER, &buffer_bindings[4] },
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[5] }, { DML_BINDING_TYPE_BUFFER, &buffer_bindings[6] },
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[7] }, { DML_BINDING_TYPE_BUFFER, &buffer_bindings[8] },
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[9] }, { DML_BINDING_TYPE_BUFFER, &buffer_bindings[10] },
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[11] }, { DML_BINDING_TYPE_BUFFER, &buffer_bindings[12] },
				{ DML_BINDING_TYPE_BUFFER, &buffer_bindings[13] }, // last layer has no bias
			};
			dml_binding_table->BindInputs(ARRAYSIZE(input_bindings), input_bindings);
		}

		DML_BUFFER_BINDING output_binding = { model_output->GetNative(), 0, model_output->GetSize() };
		DML_BINDING_DESC binding_desc{ DML_BINDING_TYPE_BUFFER, &output_binding };
		dml_binding_table->BindOutputs(1, &binding_desc);
	}

	void DirectMLUpscalerPass::AddTextureToTensorPass(RenderGraph& rg, PostProcessor const* postprocessor)
	{
		struct TextureToTensorPassData
		{
			RGBufferReadWriteId tensor;
			RGTextureReadOnlyId texture;
		};

		rg.AddPass<TextureToTensorPassData>("Texture To Tensor Pass",
			[=](TextureToTensorPassData& data, RenderGraphBuilder& builder)
			{
				data.tensor = builder.WriteBuffer(RG_NAME(ModelInput));
				data.texture = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
			},
			[=](TextureToTensorPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadWriteBuffer(data.tensor),
					ctx.GetReadOnlyTexture(data.texture),
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();

				GfxTextureDesc texture_desc = ctx.GetTexture(*data.texture).GetDesc();

				struct  TextureToTensorConstants
				{
					Vector2  resolution;
					Bool32   nhwc;
					Uint32   output_idx;
					Uint32   input_idx;
				} constants =
				{
					.resolution = Vector2((Float)texture_desc.width, (Float)texture_desc.height),
					.nhwc = tensor_layout == TensorLayout::NHWC,
					.output_idx = i,
					.input_idx = i + 1,
				};

				cmd_list->SetPipelineState(texture_to_tensor_pso.get());
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(texture_desc.width, 16), DivideAndRoundUp(texture_desc.height, 16), 1);
			}, RGPassType::Compute);
	}

	void DirectMLUpscalerPass::AddUpscalingPass(RenderGraph& rg)
	{
		struct MLUpscalerPassData
		{
			RGBufferReadOnlyId  input;
			RGBufferReadWriteId output;
		};

		rg.AddPass<MLUpscalerPassData>("DirectML Upscaler Pass",
			[=](MLUpscalerPassData& data, RenderGraphBuilder& builder) 
			{
				builder.DummyReadBuffer(RG_NAME(ModelInput));
				builder.DummyWriteBuffer(RG_NAME(ModelOutput));
			},
			[=](MLUpscalerPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				cmd_list->SetHeap(dml_heap.get());
				dml_command_recorder->RecordDispatch(cmd_list->GetNative(), dml_graph.Get(), dml_binding_table.Get());
				cmd_list->ResetState();
			}, RGPassType::Compute);
	}

	void DirectMLUpscalerPass::AddTensorToTexturePass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		struct TensorToTexturePassData
		{
			RGBufferReadOnlyId tensor;
			RGTextureReadWriteId texture;
		};

		rg.AddPass<TensorToTexturePassData>("Texture To Tensor Pass",
			[=](TensorToTexturePassData& data, RenderGraphBuilder& builder)
			{
				RGTextureDesc dml_desc{};
				dml_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				dml_desc.width = display_width;
				dml_desc.height = display_height;
				dml_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_NAME(DMLUpscalerOutput), dml_desc);

				data.tensor = builder.ReadBuffer(RG_NAME(ModelOutput));
				data.texture = builder.WriteTexture(RG_NAME(DMLUpscalerOutput));
			},
			[=](TensorToTexturePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				GfxDescriptor src_descriptors[] =
				{
					ctx.GetReadWriteTexture(data.texture),
					ctx.GetReadOnlyBuffer(data.tensor),
				};
				GfxDescriptor dst_descriptor = gfx->AllocateDescriptorsGPU(ARRAYSIZE(src_descriptors));
				gfx->CopyDescriptors(dst_descriptor, src_descriptors);
				Uint32 const i = dst_descriptor.GetIndex();
				GfxTextureDesc texture_desc = ctx.GetTexture(*data.texture).GetDesc();

				struct  TextureToTensorConstants
				{
					Vector2  resolution;
					Bool32   nhwc;
					Uint32   output_idx;
					Uint32   input_idx;
				} constants =
				{
					.resolution = Vector2((Float)texture_desc.width, (Float)texture_desc.height),
					.nhwc = tensor_layout == TensorLayout::NHWC,
					.output_idx = i,
					.input_idx = i + 1,
				};

				cmd_list->SetPipelineState(texture_to_tensor_pso.get());
				cmd_list->SetRootConstants(1, constants);
				cmd_list->Dispatch(DivideAndRoundUp(texture_desc.width, 16), DivideAndRoundUp(texture_desc.height, 16), 1);
			}, RGPassType::Compute);

		postprocessor->SetFinalResource(RG_NAME(DMLUpscalerOutput));
	}

}

