#pragma once
#include "GfxDefines.h"
#include "GfxTimestampProfilerFwd.h"
#include "Utilities/Singleton.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;

	class GfxProfiler
	{
	public:
		virtual ~GfxProfiler() = default;
		virtual void Initialize(GfxDevice* gfx) = 0;
		virtual void Shutdown() = 0;
		virtual void NewFrame() = 0;
		virtual void BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active = true) = 0;
		virtual void EndProfileScope(GfxCommandList* cmd_list) = 0;
		virtual GfxProfilerTree const* GetProfilerTree() const = 0;
	};

	class GfxProfilerManager : public Singleton<GfxProfilerManager>
	{
		friend class Singleton<GfxProfilerManager>;
	public:
		void Initialize(GfxDevice* gfx);
		void Shutdown();
		void NewFrame();
		void BeginProfileScope(GfxCommandList* cmd_list, Char const* name, Bool active = true);
		void EndProfileScope(GfxCommandList* cmd_list);
		GfxProfilerTree const* GetProfilerTree() const;

	private:
		std::unique_ptr<GfxProfiler> timestamp_profiler;
		std::unique_ptr<GfxProfiler> tracy_profiler;

	private:
		GfxProfilerManager();
		~GfxProfilerManager();
	};
	#define g_GfxProfiler GfxProfilerManager::Get()
}