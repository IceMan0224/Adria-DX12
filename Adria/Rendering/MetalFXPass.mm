#include "MetalFXPass.h"
#if defined(ADRIA_PLATFORM_MACOS)
#include <MetalFX/MetalFX.h>
#include "BlackboardData.h"
#include "Postprocessor.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/Metal/MetalDevice.h"
#include "Graphics/Metal/MetalCommandList.h"
#include "Graphics/Metal/MetalTexture.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "Core/ConsoleManager.h"
#include "Logging/Log.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(PostProcessor);

	static Float const UpscaleRatios[] = { 1.5f, 1.7f, 2.0f, 3.0f };
	static Char const* QualityModeNames = "Quality (1.5x)\0Balanced (1.7x)\0Performance (2.0x)\0Ultra Performance (3.0x)\0";

	MetalFXPass::MetalFXPass(GfxDevice* _gfx, Uint32 w, Uint32 h)
		: gfx(_gfx), display_width(w), display_height(h), render_width(w), render_height(h)
	{
		is_supported = gfx->GetBackend() == GfxBackend::Metal;
		if (!is_supported)
		{
			ADRIA_LOG(WARNING, "MetalFX is only supported on the Metal backend");
			return;
		}
		RecreateRenderResolution();
	}

	MetalFXPass::~MetalFXPass()
	{
		DestroyScalers();
	}

	void MetalFXPass::AddPass(RenderGraph& rg, PostProcessor* postprocessor)
	{
		if (!IsSupported()) return;

		if (recreate_scalers)
		{
			DestroyScalers();
			CreateScalers();
		}

		FrameBlackboardData const& frame_data = rg.GetBlackboard().Get<FrameBlackboardData>();

		if (mode == MetalFXMode::Spatial)
		{
			struct MetalFXSpatialPassData
			{
				RGTextureReadOnlyId  input;
				RGTextureReadWriteId output;
			};

			rg.AddPass<MetalFXSpatialPassData>("MetalFX Spatial",
				[=, this](MetalFXSpatialPassData& data, RenderGraphBuilder& builder)
				{
					RGTextureDesc out_desc{};
					out_desc.format = GfxFormat::R16G16B16A16_FLOAT;
					out_desc.width  = display_width;
					out_desc.height = display_height;
					out_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
					builder.DeclareTexture(RG_NAME(MetalFXOutput), out_desc);

					data.output = builder.WriteTexture(RG_NAME(MetalFXOutput));
					data.input  = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
				},
				[=, this](MetalFXSpatialPassData const& data, RenderGraphContext& ctx)
				{
					GfxCommandList* cmd_list = ctx.GetCommandList();
					GfxTexture& input_texture  = ctx.GetTexture(*data.input);
					GfxTexture& output_texture = ctx.GetTexture(*data.output);

					id<MTLTexture> mtl_input  = static_cast<MetalTexture*>(&input_texture)->GetMetalTexture();
					id<MTLTexture> mtl_output = static_cast<MetalTexture*>(&output_texture)->GetMetalTexture();

					id<MTLFXSpatialScaler> scaler = (__bridge id<MTLFXSpatialScaler>)spatial_scaler;
					scaler.colorTexture       = mtl_input;
					scaler.outputTexture      = mtl_output;
					scaler.inputContentWidth  = render_width;
					scaler.inputContentHeight = render_height;

					MetalCommandList* mtl_cmd_list = static_cast<MetalCommandList*>(cmd_list);
					mtl_cmd_list->EndAllEncoders();
					id<MTLCommandBuffer> cmd_buf = mtl_cmd_list->GetCommandBuffer();
					[scaler encodeToCommandBuffer:cmd_buf];
				}, RGPassType::Compute);

		}
		else
		{
			struct MetalFXTemporalPassData
			{
				RGTextureReadOnlyId  input;
				RGTextureReadOnlyId  depth;
				RGTextureReadOnlyId  velocity;
				RGTextureReadWriteId output;
			};

			rg.AddPass<MetalFXTemporalPassData>("MetalFX Temporal",
				[=, this](MetalFXTemporalPassData& data, RenderGraphBuilder& builder)
				{
					RGTextureDesc out_desc{};
					out_desc.format = GfxFormat::R16G16B16A16_FLOAT;
					out_desc.width  = display_width;
					out_desc.height = display_height;
					out_desc.clear_value = GfxClearValue(0.0f, 0.0f, 0.0f, 0.0f);
					builder.DeclareTexture(RG_NAME(MetalFXOutput), out_desc);

					data.output   = builder.WriteTexture(RG_NAME(MetalFXOutput));
					data.input    = builder.ReadTexture(postprocessor->GetFinalResource(), ReadAccess_NonPixelShader);
					data.velocity = builder.ReadTexture(RG_NAME(VelocityBuffer), ReadAccess_NonPixelShader);
					data.depth    = builder.ReadTexture(RG_NAME(DepthStencil), ReadAccess_NonPixelShader);
				},
				[=, this](MetalFXTemporalPassData const& data, RenderGraphContext& ctx)
				{
					GfxCommandList* cmd_list = ctx.GetCommandList();
					GfxTexture& input_texture    = ctx.GetTexture(*data.input);
					GfxTexture& depth_texture    = ctx.GetTexture(*data.depth);
					GfxTexture& velocity_texture = ctx.GetTexture(*data.velocity);
					GfxTexture& output_texture   = ctx.GetTexture(*data.output);

					id<MTLTexture> mtl_input    = static_cast<MetalTexture*>(&input_texture)->GetMetalTexture();
					id<MTLTexture> mtl_depth    = static_cast<MetalTexture*>(&depth_texture)->GetMetalTexture();
					id<MTLTexture> mtl_velocity = static_cast<MetalTexture*>(&velocity_texture)->GetMetalTexture();
					id<MTLTexture> mtl_output   = static_cast<MetalTexture*>(&output_texture)->GetMetalTexture();

					id<MTLFXTemporalScaler> scaler = (__bridge id<MTLFXTemporalScaler>)temporal_scaler;
					scaler.colorTexture       = mtl_input;
					scaler.depthTexture       = mtl_depth;
					scaler.motionTexture      = mtl_velocity;
					scaler.outputTexture      = mtl_output;
					scaler.jitterOffsetX      = frame_data.camera_jitter_x;
					scaler.jitterOffsetY      = frame_data.camera_jitter_y;
					scaler.motionVectorScaleX = (float)render_width;
					scaler.motionVectorScaleY = (float)render_height;
					scaler.reset              = NO;

					MetalCommandList* mtl_cmd_list = static_cast<MetalCommandList*>(cmd_list);
					mtl_cmd_list->EndAllEncoders();
					id<MTLCommandBuffer> cmd_buf = mtl_cmd_list->GetCommandBuffer();
					[scaler encodeToCommandBuffer:cmd_buf];
				}, RGPassType::Compute);
		}

		postprocessor->SetFinalResource(RG_NAME(MetalFXOutput));
	}

	void MetalFXPass::OnResize(Uint32 w, Uint32 h)
	{
		display_width = w;
		display_height = h;
		RecreateRenderResolution();
		recreate_scalers = true;
	}

	Bool MetalFXPass::IsEnabled(PostProcessor const*) const
	{
		return true;
	}

	Bool MetalFXPass::IsSupported() const
	{
		return is_supported;
	}

	void MetalFXPass::GUI()
	{
		QueueGUI([&]()
		{
			if (ImGui::TreeNodeEx("MetalFX", ImGuiTreeNodeFlags_None))
			{
				Int current_mode = (Int)mode;
				if (ImGui::Combo("Mode", &current_mode, "Spatial\0Temporal\0", 2))
				{
					mode = (MetalFXMode)current_mode;
					recreate_scalers = true;
				}

				Int quality_int = (Int)quality;
				if (ImGui::Combo("Quality Mode", &quality_int, QualityModeNames, 4))
				{
					quality = (MetalFXQuality)quality_int;
					RecreateRenderResolution();
					recreate_scalers = true;
				}

				ImGui::TreePop();
			}
		}, GUICommandGroup_PostProcessing, GUICommandSubGroup_Upscaler);
	}

	void MetalFXPass::CreateScalers()
	{
		MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
		id<MTLDevice> device = metal_device->GetMTLDevice();

		if (mode == MetalFXMode::Spatial)
		{
			MTLFXSpatialScalerDescriptor* desc = [[MTLFXSpatialScalerDescriptor alloc] init];
			desc.inputWidth          = render_width;
			desc.inputHeight         = render_height;
			desc.outputWidth         = display_width;
			desc.outputHeight        = display_height;
			desc.colorTextureFormat  = MTLPixelFormatRGBA16Float;
			desc.outputTextureFormat = MTLPixelFormatRGBA16Float;
			desc.colorProcessingMode = MTLFXSpatialScalerColorProcessingModePerceptual;

			id<MTLFXSpatialScaler> scaler = [desc newSpatialScalerWithDevice:device];
			if (!scaler)
			{
				ADRIA_LOG(ERROR, "Failed to create MTLFXSpatialScaler");
				is_supported = false;
				return;
			}
			spatial_scaler = (__bridge_retained void*)scaler;
		}
		else
		{
			MTLFXTemporalScalerDescriptor* desc = [[MTLFXTemporalScalerDescriptor alloc] init];
			desc.inputWidth              = render_width;
			desc.inputHeight             = render_height;
			desc.outputWidth             = display_width;
			desc.outputHeight            = display_height;
			desc.colorTextureFormat      = MTLPixelFormatRGBA16Float;
			desc.depthTextureFormat      = MTLPixelFormatDepth32Float;
			desc.motionTextureFormat     = MTLPixelFormatRG16Float;
			desc.outputTextureFormat     = MTLPixelFormatRGBA16Float;
			desc.autoExposureEnabled     = NO;
			desc.inputContentPropertiesEnabled = NO;

			id<MTLFXTemporalScaler> scaler = [desc newTemporalScalerWithDevice:device];
			if (!scaler)
			{
				ADRIA_LOG(ERROR, "Failed to create MTLFXTemporalScaler");
				is_supported = false;
				return;
			}
			temporal_scaler = (__bridge_retained void*)scaler;
		}

		recreate_scalers = false;
	}

	void MetalFXPass::DestroyScalers()
	{
		if (spatial_scaler)
		{
			id<MTLFXSpatialScaler> scaler = (__bridge_transfer id<MTLFXSpatialScaler>)spatial_scaler;
			(void)scaler; 
			spatial_scaler = nullptr;
		}
		if (temporal_scaler)
		{
			id<MTLFXTemporalScaler> scaler = (__bridge_transfer id<MTLFXTemporalScaler>)temporal_scaler;
			(void)scaler; 
			temporal_scaler = nullptr;
		}
	}

	void MetalFXPass::RecreateRenderResolution()
	{
		Float upscale_ratio = UpscaleRatios[(Uint32)quality];
		render_width  = (Uint32)((Float)display_width  / upscale_ratio);
		render_height = (Uint32)((Float)display_height / upscale_ratio);
		BroadcastRenderResolutionChanged(render_width, render_height);
	}
}
#endif
