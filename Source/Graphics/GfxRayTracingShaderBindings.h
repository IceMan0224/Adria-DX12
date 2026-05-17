#pragma once
#include "GfxDefines.h"

namespace adria
{
	class GfxRayTracingPipeline;

	struct GfxShaderGroupHandle
	{
		Uint32 index = UINT32_MAX;

		GfxShaderGroupHandle() = default;
		explicit GfxShaderGroupHandle(Uint32 _index) : index(_index) {}

		Bool IsValid() const { return index != UINT32_MAX; }
	};

	class GfxRayTracingShaderBindings
	{
	public:
		virtual ~GfxRayTracingShaderBindings() = default;

		virtual void SetRayGenShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) = 0;
		virtual GfxShaderGroupHandle AddMissShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) = 0;
		virtual GfxShaderGroupHandle AddHitGroup(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) = 0;
		virtual GfxShaderGroupHandle AddCallableShader(Char const* name, void const* local_data = nullptr, Uint32 data_size = 0) = 0;

		virtual void Commit() = 0;

		virtual Uint32 GetMissShaderIndex(GfxShaderGroupHandle handle) const = 0;
		virtual Uint32 GetHitGroupIndex(GfxShaderGroupHandle handle) const = 0;
		virtual Uint32 GetCallableShaderIndex(GfxShaderGroupHandle handle) const = 0;

	protected:
		GfxRayTracingShaderBindings() = default;
		ADRIA_NONCOPYABLE(GfxRayTracingShaderBindings)
	};
}