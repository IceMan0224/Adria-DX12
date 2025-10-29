#pragma once
#include "PostEffect.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class UpscalerPass : public PostEffect
	{
		DECLARE_EVENT(RenderResolutionChanged, UpscalerPass, Uint32, Uint32)
	public:
		RenderResolutionChanged& GetRenderResolutionChangedEvent() { return render_resolution_changed_event; }

	private:
		RenderResolutionChanged render_resolution_changed_event;

	protected:
		void BroadcastRenderResolutionChanged(Uint32 w, Uint32 h)
		{
			render_resolution_changed_event.Broadcast(w, h);
		}
	};

	class DummyUpscalerPass : public UpscalerPass
	{
	public:
		DummyUpscalerPass() {}
		virtual void AddPass(RenderGraph&, PostProcessor*) override {}
		virtual void OnResize(Uint32, Uint32) override {}
        virtual Bool IsEnabled(PostProcessor const*) const override { return false; }
        virtual Bool IsSupported() const override { return false; }
	};
}
