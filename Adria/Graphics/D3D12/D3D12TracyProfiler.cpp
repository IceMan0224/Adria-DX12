#include "D3D12TracyProfiler.h"
#include "D3D12Device.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxCommandQueue.h"

namespace adria
{
	void D3D12TracyProfiler::Initialize(GfxDevice* gfx)
	{
#if GFX_PROFILING_USE_TRACY
		ADRIA_ASSERT_MSG(gfx->GetBackend() == GfxBackend::D3D12, "Tracy D3D12 profiler can only be used with D3D12 backend!");
		D3D12Device* d3d12_gfx = static_cast<D3D12Device*>(gfx);
		GfxCommandQueue* cmd_queue = d3d12_gfx->GetGraphicsCommandQueue();
		tracy_ctx = TracyD3D12Context(d3d12_gfx->GetD3D12Device(), (ID3D12CommandQueue*)cmd_queue->GetNative());
#endif
	}

	void D3D12TracyProfiler::Shutdown()
	{
#if GFX_PROFILING_USE_TRACY
		active_zones.clear();
		if (tracy_ctx)
		{
			TracyD3D12Destroy(tracy_ctx);
			tracy_ctx = nullptr;
		}
#endif
	}

	void D3D12TracyProfiler::NewFrame()
	{
#if GFX_PROFILING_USE_TRACY
		if (tracy_ctx)
		{
			TracyD3D12Collect(tracy_ctx);
			TracyD3D12NewFrame(tracy_ctx);
		}
		active_zones.clear();
#endif
	}

	void D3D12TracyProfiler::BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active)
	{
#if GFX_PROFILING_USE_TRACY
		if (!tracy_ctx || !active)
		{
			return;
		}

#if GFX_MULTITHREADED
		std::lock_guard lock(tracy_mutex);
#endif

		ID3D12GraphicsCommandList* d3d12_cmd_list = static_cast<ID3D12GraphicsCommandList*>(cmd_list->GetNative());
		auto zone = std::make_unique<TracyZoneScope>(tracy_ctx, d3d12_cmd_list, name, active);
		active_zones[cmd_list].push_back(std::move(zone));
#endif
	}

	void D3D12TracyProfiler::EndProfileScope(GfxCommandList* cmd_list)
	{
#if GFX_PROFILING_USE_TRACY
		if (!tracy_ctx)
		{
			return;
		}

#if GFX_MULTITHREADED
		std::lock_guard lock(tracy_mutex);
#endif

		auto it = active_zones.find(cmd_list);
		if (it != active_zones.end() && !it->second.empty())
		{
			it->second.pop_back();
			if (it->second.empty())
			{
				active_zones.erase(it);
			}
		}
		else
		{
			ADRIA_ASSERT_MSG(false, "EndProfileScope called without matching BeginProfileScope!");
		}
#endif
	}

	GfxProfilerTree const* D3D12TracyProfiler::GetProfilerTree() const
	{
		return nullptr;
	}
}