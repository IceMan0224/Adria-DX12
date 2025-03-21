#include "DirectMLUpscalerPass.h"
#include "ShaderManager.h"
#include "BlackboardData.h"
#include "Postprocessor.h"
#include "Core/Paths.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"
#include "Utilities/FloatCompressor.h"

//reference: DirectML sample (add link)

namespace adria
{
	namespace
	{
		constexpr Uint32 BUFFER_LENGTH = 256;
		Bool LoadWeights(std::string const& weights_file_name, std::unordered_map<std::string, std::vector<Float>>& weight_map)
		{
			std::ifstream input(paths::MLDir + weights_file_name, std::ifstream::binary);
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

			Uint32 name_len;
			Uint32 w_len;
			Char name_buf[BUFFER_LENGTH];
			try
			{
				while (weight_tensor_count--)
				{
					input.read(reinterpret_cast<Char*>(&name_len), sizeof(name_len));
					if (name_len > BUFFER_LENGTH - 1)
					{
						ADRIA_ERROR("[DirectML] name_len exceeds BUFFER_LENGTH");
						return false;
					}
					input.read(name_buf, name_len);
					name_buf[name_len] = '\0';
					std::string name(name_buf);

					input.read(reinterpret_cast<Char*>(&w_len), sizeof(Uint32));
					weight_map[name] = std::vector<Float>(w_len);
					input.read(reinterpret_cast<Char*>(weight_map[name].data()), sizeof(Float) * w_len);
					ADRIA_DEBUG("[DirectML] Loaded Tensor: %s -> %d", name.c_str(), w_len);
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
		std::pair<std::vector<Uint16>, Uint64> ConvertWeights(std::vector<Float> const& filter_weights, std::vector<Float> const& scale_weights, std::vector<Float> const& shift_weights, std::span<const Uint32> sizes, Bool is_filter)
		{
			std::vector<Uint16> result;
			Uint32 N = sizes[0], C = sizes[1], H = sizes[2], W = sizes[3];
			Uint64 total_size = N * C * H * W;
			if (is_filter)
			{
				result.reserve(total_size);
				for (Uint32 n = 0; n < N; ++n)
				{
					for (Uint32 i = 0; i < C * H * W; ++i)
					{
						Uint32 idx = n * C * H * W + i;
						Float scaled_weight = scale_weights.empty() ?
							filter_weights[idx] :
							filter_weights[idx] * scale_weights[n];
						result.push_back(FloatCompressor::Compress(scaled_weight));
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
	}

	static TAutoConsoleVariable<Bool> DirectML("r.DirectML", true, "Enable or Disable DirectML Upscaler");

	DirectMLUpscalerPass::DirectMLUpscalerPass(GfxDevice* gfx, Uint32 w, Uint32 h) : gfx(gfx), display_width(0), display_height(0)
	{
		ADRIA_TODO("Add cmd line option for debug dml device");
		Bool const DEBUG_DML_DEVICE = false;
		GFX_CHECK_HR(DMLCreateDevice(gfx->GetDevice(), DEBUG_DML_DEVICE ? DML_CREATE_DEVICE_FLAG_DEBUG : DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(m_dmlDevice.GetAddressOf())));

		ADRIA_TODO("Pick layout depending on gfx vendor, if nvidia, pick NHWC");
		m_tensorLayout = TensorLayout::Default;

		DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT fp16_query = { DML_TENSOR_DATA_TYPE_FLOAT16 };
		DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT fp16_supported = {};
		GFX_CHECK_HR(m_dmlDevice->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT, sizeof(fp16_query), &fp16_query, sizeof(fp16_supported), &fp16_supported));

		if (!fp16_supported.IsSupported)
		{
			return;
		}
		supported = true;

		GFX_CHECK_HR(m_dmlDevice->CreateCommandRecorder(IID_PPV_ARGS(m_dmlCommandRecorder.GetAddressOf())));

		CreatePSOs();
		OnResize(w, h);
	}

	void DirectMLUpscalerPass::OnResize(Uint32 w, Uint32 h)
	{
		display_width = w, display_height = h;
		CreateDirectMLResources();
	}

	void DirectMLUpscalerPass::OnSceneInitialized()
	{
		InitializeDirectMLResources();
	}

	void DirectMLUpscalerPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		struct DirectMLUpscalerPassData
		{

		};

		rg.AddPass<DirectMLUpscalerPassData>("DirectML Upscaler Pass",
			[=](DirectMLUpscalerPassData& data, RenderGraphBuilder& builder)
			{
			},
			[=](DirectMLUpscalerPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
			}, RGPassType::Compute);

		postprocessor->SetFinalResource(RG_NAME(DirectMLOutput));
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
		uint64_t modelInputBufferSize = 0;
		uint64_t modelOutputBufferSize = 0;
		uint64_t intermediateBufferMaxSize[] = { 0, 0 };

		Uint32 render_width = display_width * 0.5f;
		Uint32 render_height = display_height * 0.5f;
		// Create an upscaled (nearest neighbor) version of the image first
		uint32_t modelInputSizes[] = { 1, 3, render_height, render_width };
		uint32_t upscaledInputSizes[4];

		using Dimensions = dml::TensorDesc::Dimensions;

		std::unordered_map<std::string, std::vector<Float>> weights;
		if (!LoadWeights("weights.bin", weights))
		{
			ADRIA_ASSERT_MSG(false, "[DIRECT ML] Weight loading failed!");
		}

	}

	void DirectMLUpscalerPass::InitializeDirectMLResources()
	{
		GFX_CHECK_HR(m_dmlDevice->CreateOperatorInitializer(1, m_dmlGraph.GetAddressOf(), IID_PPV_ARGS(m_dmlOpInitializer.ReleaseAndGetAddressOf())));

		DML_BINDING_PROPERTIES initBindingProps = m_dmlOpInitializer->GetBindingProperties();
		DML_BINDING_PROPERTIES executeBindingProps = m_dmlGraph->GetBindingProperties();

	}

}

