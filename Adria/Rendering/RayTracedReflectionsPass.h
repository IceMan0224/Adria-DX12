#pragma once
#include "PostEffect.h"
#include "BlurPass.h"
#include "HelperPasses.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxRayTracingPipeline;
	class GfxShaderKey;

	class RayTracedReflectionsPass : public PostEffect
	{
	public:
		RayTracedReflectionsPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~RayTracedReflectionsPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;
		virtual Bool IsSupported() const override;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxRayTracingPipeline> ray_traced_reflections_pso;
		Uint32 width, height;
		BlurPass blur_pass;

		Bool is_supported;
		Float reflection_roughness_scale = 0.0f;
		CopyToTexturePass copy_to_texture_pass;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}