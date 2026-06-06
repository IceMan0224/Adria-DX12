#pragma once
#include "DDGIPass.h"
#include "ReSTIR_GI.h"

namespace adria
{
	class GfxDevice;
	class RenderGraph;

	class GIManager
	{
	public:
		GIManager(GfxDevice* gfx, entt::registry& reg, Uint32 width, Uint32 height);

		void AddPasses(RenderGraph& rg);
		void AddVisualizePass(RenderGraph& rg);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);
		void OnSceneInitialized();
		Int32 GetDDGIVolumeIndex();

	private:
		GfxDevice* gfx;
		DDGIPass  ddgi;
		ReSTIR_GI restir_gi;
	};
}
