#pragma once
#include "SSAOPass.h"
#include "HBAOPass.h"
#include "RayTracedAmbientOcclusionPass.h"

namespace adria
{
	class GfxDevice;
	class AmbientOcclusionManager
	{
	public:
		AmbientOcclusionManager(GfxDevice* gfx, Uint32 w, Uint32 h);
		~AmbientOcclusionManager();

		void AddPass(RenderGraph&);
		void OnResize(Uint32 w, Uint32 h);
		void OnSceneInitialized();
		void GUI();

	private:
		GfxDevice*	 gfx;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		RayTracedAmbientOcclusionPass rtao_pass;
	};
}

