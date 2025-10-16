#pragma once
#include "GfxFormat.h"
#include "Utilities/Enum.h"

namespace adria
{
	class GfxBuffer;
	class GfxDevice;
	class GfxRayTracingBLAS;

	enum GfxRayTracingASFlagBit : Uint32
	{
		GfxRayTracingASFlag_None = 0,
		GfxRayTracingASFlag_AllowUpdate = BIT(0),
		GfxRayTracingASFlag_AllowCompaction = BIT(1),
		GfxRayTracingASFlag_PreferFastTrace = BIT(2),
		GfxRayTracingASFlag_PreferFastBuild = BIT(3),
		GfxRayTracingASFlag_MinimizeMemory = BIT(4),
		GfxRayTracingASFlag_PerformUpdate = BIT(5)
	};
	using GfxRayTracingASFlags = Uint32;

	enum GfxRayTracingInstanceFlagBit : Uint32
	{
		GfxRayTracingInstanceFlag_None = BIT(0),
		GfxRayTracingInstanceFlag_CullDisable = BIT(1),
		GfxRayTracingInstanceFlag_FrontCCW = BIT(2),
		GfxRayTracingInstanceFlag_ForceOpaque = BIT(3),
		GfxRayTracingInstanceFlag_ForceNoOpaque = BIT(4)
	};
	using GfxRayTracingInstanceFlags = Uint32;

	struct GfxRayTracingGeometry
	{
		GfxBuffer* vertex_buffer = nullptr;
		Uint32 vertex_buffer_offset = 0;
		Uint32 vertex_count = 0;
		Uint32 vertex_stride = 0;
		GfxFormat vertex_format = GfxFormat::UNKNOWN;

		GfxBuffer* index_buffer = nullptr;
		Uint32 index_buffer_offset = 0;
		Uint32 index_count = 0;
		GfxFormat index_format = GfxFormat::UNKNOWN;

		Bool opaque = true;
	};

	class GfxRayTracingBLAS
	{
	public:
		virtual ~GfxRayTracingBLAS() = default;
		virtual Uint64 GetGpuAddress() const = 0;
		virtual GfxBuffer const& GetBuffer() const = 0;

		GfxBuffer const& operator*() const { return GetBuffer(); }
	};

	struct GfxRayTracingInstance
	{
		GfxRayTracingBLAS* blas = nullptr;
		Float transform[4][4] = {};
		Uint32 instance_id = 0;
		Uint8 instance_mask = 0xFF;
		GfxRayTracingInstanceFlags flags = GfxRayTracingInstanceFlag_None;
	};

	class GfxRayTracingTLAS
	{
	public:
		virtual ~GfxRayTracingTLAS() = default;
		virtual Uint64 GetGpuAddress() const = 0;
		virtual GfxBuffer const& GetBuffer() const = 0;

		GfxBuffer const& operator*() const { return GetBuffer(); }
	};
}