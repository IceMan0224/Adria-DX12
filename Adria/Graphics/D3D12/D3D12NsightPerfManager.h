#pragma once
#include "Graphics/GfxNsightPerfManager.h"
#include "Graphics/GfxDefines.h"

namespace adria
{
	class GfxDevice;
	class D3D12NsightPerfReporter;
	class D3D12NsightPerfHUD;

	class D3D12NsightPerfManager : public GfxNsightPerfManager
	{
	public:
		D3D12NsightPerfManager(GfxDevice* gfx, GfxNsightPerfMode perf_mode);
		virtual ~D3D12NsightPerfManager() override;

		virtual void Update() override;
		virtual void BeginFrame() override;
		virtual void Render() override;
		virtual void EndFrame() override;
		virtual void PushRange(GfxCommandList*, Char const*) override;
		virtual void PopRange(GfxCommandList*) override;
		virtual void GenerateReport() override;

	private:
#if defined(GFX_ENABLE_NV_PERF)
		std::unique_ptr<D3D12NsightPerfReporter> perf_reporter;
		std::unique_ptr<D3D12NsightPerfHUD>      perf_hud;
#endif
	};
}