#pragma once
#include "GfxShader.h"

namespace adria
{
	class GfxDevice;

	class GfxCapabilities
	{
	public:

		virtual ~GfxCapabilities() = default;
		virtual Bool Initialize(GfxDevice* gfx) = 0;

		Bool SupportsHardwareRayTracing() const
		{
			return hardware_ray_tracing_supported;
		}
		Bool SupportsInlineRayTracing() const
		{
			return inline_ray_tracing_supported;
		}
		Bool SupportsMeshShaders() const
		{
			return mesh_shaders_supported;
		}
		Bool SupportsVariableRateShading() const
		{
			return variable_rate_shading_supported;
		}
		Bool SupportsVariableRateShadingImage() const
		{
			return variable_rate_shading_image_supported;
		}
		Bool SupportsWorkGraphs() const
		{
			return work_graphs_supported;
		}

		Bool SupportsShaderModel(GfxShaderModel sm) const
		{
			return shader_model >= sm;
		}
		Bool SupportsEnhancedBarriers() const
		{
			return enhanced_barriers_supported;
		}
		Bool SupportsTypedUAVLoadAdditionalFormats() const
		{
			return typed_uav_additional_formats_supported;
		}

		Bool SupportsAdditionalShadingRates() const { return additional_shading_rates_supported; }
		Uint32 GetShadingRateImageTileSize() const { return shading_rate_image_tile_size; }

	protected:
		Bool hardware_ray_tracing_supported = false;
		Bool inline_ray_tracing_supported = false;
		Bool mesh_shaders_supported = false;
		Bool variable_rate_shading_supported = false;
		Bool variable_rate_shading_image_supported = false;
		Bool work_graphs_supported = false;
		GfxShaderModel shader_model = SM_Unknown;
		Bool enhanced_barriers_supported = false;
		Bool typed_uav_additional_formats_supported = false;
		Bool additional_shading_rates_supported = false;
		Uint32 shading_rate_image_tile_size = 0;
	};
}