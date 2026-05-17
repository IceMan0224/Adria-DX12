#pragma once
#include "PostEffect.h"
#include "Graphics/GfxPipelineStateFwd.h"

namespace adria
{
	class GfxDevice;

	class SilhouettePass : public PostEffect
	{
	public:
		static constexpr Uint32 kMaxSelectedIds = 128;
		static constexpr Uint32 kIdsPackedCount = (kMaxSelectedIds + 3) / 4;

		struct alignas(16) SilhouetteIdsCB
		{
			Uint32 idCount;
			Uint32 _pad0;
			Uint32 _pad1;
			Uint32 _pad2;
			Uint32 ids[kIdsPackedCount * 4];
		};

		SilhouettePass(GfxDevice* gfx, Uint32 w, Uint32 h);

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(Uint32, Uint32) override;
		virtual Bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;

	private:
		GfxDevice* gfx;
		Uint32 width, height;
		std::unique_ptr<GfxComputePipelineState> pso;
		SilhouetteIdsCB ids_cb{};

	private:
		void CreatePSO();
	};
}
