#pragma once
#include "Graphics/GfxProfiler.h"

#if GFX_PROFILING_USE_TRACY
#include "tracy/TracyD3D12.hpp"
#include <unordered_map>
#include <memory>
#endif

namespace adria
{
	class D3D12TracyProfiler final : public GfxProfiler
	{
	public:
		~D3D12TracyProfiler() override = default;

		void Initialize(GfxDevice* gfx) override;
		void Shutdown() override;
		void NewFrame() override;
		void BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active = true) override;
		void EndProfileScope(GfxCommandList* cmd_list) override;
		GfxProfilerTree const* GetProfilerTree() const override;

	private:
#if GFX_PROFILING_USE_TRACY
		TracyD3D12Ctx tracy_ctx = nullptr;
		struct TracyZoneScope
		{
			tracy::D3D12ZoneScope zone;
			TracyZoneScope(TracyD3D12Ctx ctx, ID3D12GraphicsCommandList* cmd_list, Char const* name, Bool active)
				: zone(ctx,
					__LINE__, __FILE__, strlen(__FILE__),
					"", 0, 
					name, strlen(name),
					cmd_list, active)
			{
			}
		};
		std::unordered_map<GfxCommandList*, std::vector<std::unique_ptr<TracyZoneScope>>> active_zones;
#if GFX_MULTITHREADED
		std::mutex tracy_mutex;
#endif
#endif
	};
}