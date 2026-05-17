#include "D3D12Capabilities.h"
#include "D3D12Device.h"
#include "d3dx12_check_feature_support.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(Graphics);

	namespace
	{
		constexpr GfxShaderModel ConvertShaderModel(D3D_SHADER_MODEL shader_model)
		{
			switch (shader_model)
			{
			case D3D_SHADER_MODEL_6_0: return SM_6_0;
			case D3D_SHADER_MODEL_6_1: return SM_6_1;
			case D3D_SHADER_MODEL_6_2: return SM_6_2;
			case D3D_SHADER_MODEL_6_3: return SM_6_3;
			case D3D_SHADER_MODEL_6_4: return SM_6_4;
			case D3D_SHADER_MODEL_6_5: return SM_6_5;
			case D3D_SHADER_MODEL_6_6: return SM_6_6;
			case D3D_SHADER_MODEL_6_7: return SM_6_7;
			case D3D_SHADER_MODEL_6_8: return SM_6_8;
			default:
				return SM_Unknown;
			}
		}
	}

	Bool D3D12Capabilities::Initialize(GfxDevice* gfx)
	{
		CD3DX12FeatureSupport feature_support;
		feature_support.Init((ID3D12Device*)gfx->GetNative());

		hardware_ray_tracing_supported = feature_support.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_0;
		inline_ray_tracing_supported = feature_support.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_1;
		variable_rate_shading_supported = feature_support.VariableShadingRateTier() >= D3D12_VARIABLE_SHADING_RATE_TIER_1;
		variable_rate_shading_image_supported = feature_support.VariableShadingRateTier() >= D3D12_VARIABLE_SHADING_RATE_TIER_2;
		mesh_shaders_supported = feature_support.MeshShaderTier() >= D3D12_MESH_SHADER_TIER_1;
		work_graphs_supported = feature_support.WorkGraphsTier() >= D3D12_WORK_GRAPHS_TIER_1_0;
		shader_model = ConvertShaderModel(feature_support.HighestShaderModel());
		enhanced_barriers_supported = feature_support.EnhancedBarriersSupported();
		typed_uav_additional_formats_supported = feature_support.TypedUAVLoadAdditionalFormats();
		shading_rate_image_tile_size = feature_support.ShadingRateImageTileSize();
		additional_shading_rates_supported = feature_support.AdditionalShadingRatesSupported();

		if (shader_model < SM_6_7)
		{
			ADRIA_LOG(ERROR, "Device doesn't support Shader Model 6.6 which is required!");
			return false;
		}

		return true;
	}
}
