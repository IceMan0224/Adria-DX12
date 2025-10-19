#include "GfxProfiler.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxQueryHeap.h"
#include "D3D12/D3D12TimestampProfiler.h"
#include "D3D12/D3D12TracyProfiler.h"

namespace adria
{
	class GfxDummyProfiler : public GfxProfiler
	{
	public:
		virtual void Initialize(GfxDevice*) override {}
		virtual void Shutdown() override {}
		virtual void NewFrame() override {}
		virtual void BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active = true) override {}
		virtual void EndProfileScope(GfxCommandList* cmd_list) override {}
		virtual GfxProfilerTree const* GetProfilerTree() const override { return nullptr; }
	};

	GfxProfilerManager::GfxProfilerManager()
	{
	}

	GfxProfilerManager::~GfxProfilerManager()
	{
	}

	void GfxProfilerManager::Initialize(GfxDevice* gfx)
	{
		GfxBackend const backend = gfx->GetBackend();
#if GFX_PROFILING
		switch (backend)
		{
		case GfxBackend::D3D12: timestamp_profiler = std::make_unique<D3D12TimestampProfiler>(); break;
		case GfxBackend::Vulkan:
		case GfxBackend::Metal:
		default:				timestamp_profiler = std::make_unique<GfxDummyProfiler>(); break;
		}
		ADRIA_ASSERT(timestamp_profiler != nullptr);
		timestamp_profiler->Initialize(gfx);
#endif

#if GFX_PROFILING_USE_TRACY
		switch (backend)
		{
		case GfxBackend::D3D12: tracy_profiler = std::make_unique<D3D12TracyProfiler>(); break;
		case GfxBackend::Vulkan:
		case GfxBackend::Metal:
		default:				tracy_profiler = std::make_unique<GfxDummyProfiler>(); break;
		}
		ADRIA_ASSERT(tracy_profiler != nullptr);
		tracy_profiler->Initialize(gfx);
#endif
	}

	void GfxProfilerManager::Shutdown()
	{
#if GFX_PROFILING
		timestamp_profiler->Shutdown();
		timestamp_profiler.reset();
#endif

#if GFX_PROFILING_USE_TRACY
		tracy_profiler->Shutdown();
		tracy_profiler.reset();
#endif
	}

	void GfxProfilerManager::NewFrame()
	{
#if GFX_PROFILING
		timestamp_profiler->NewFrame();
#endif

#if GFX_PROFILING_USE_TRACY
		tracy_profiler->NewFrame();
#endif
	}

	void GfxProfilerManager::BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active)
	{
#if GFX_PROFILING
		timestamp_profiler->BeginProfileScope(cmd_list, name, active);
#endif

#if GFX_PROFILING_USE_TRACY
		tracy_profiler->BeginProfileScope(cmd_list, name, active);
#endif
	}

	void GfxProfilerManager::EndProfileScope(GfxCommandList* cmd_list)
	{
#if GFX_PROFILING
		if (timestamp_profiler)
		{
			timestamp_profiler->EndProfileScope(cmd_list);
		}
#endif

#if GFX_PROFILING_USE_TRACY
		if (tracy_profiler)
		{
			tracy_profiler->EndProfileScope(cmd_list);
		}
#endif
	}

	GfxProfilerTree const* GfxProfilerManager::GetProfilerTree() const
	{
#if GFX_PROFILING
		ADRIA_ASSERT(timestamp_profiler != nullptr);
		return timestamp_profiler->GetProfilerTree();
#endif
		return nullptr;
	}
}