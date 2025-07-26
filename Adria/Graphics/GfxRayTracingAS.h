#pragma once
#include "GfxFormat.h"
#include "Utilities/EnumUtil.h"

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
		GfxBuffer* vertex_buffer;
		Uint32 vertex_buffer_offset;
		Uint32 vertex_count;
		Uint32 vertex_stride;
		GfxFormat vertex_format;

		GfxBuffer* index_buffer;
		Uint32 index_buffer_offset;
		Uint32 index_count;
		GfxFormat index_format;

		Bool opaque;
	};

	class GfxRayTracingBLAS
	{
	public:
		GfxRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
		~GfxRayTracingBLAS();

		Uint64 GetGpuAddress() const;
		GfxBuffer const& GetBuffer() const { return *result_buffer; }
		GfxBuffer const& operator*() const { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
	};


	struct GfxRayTracingInstance
	{
		GfxRayTracingBLAS* blas;
		Float transform[4][4];
		Uint32 instance_id;
		Uint8 instance_mask;
		GfxRayTracingInstanceFlags flags;
	};

	class GfxRayTracingTLAS
	{
	public:
		GfxRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
		~GfxRayTracingTLAS();

		Uint64 GetGpuAddress() const;
		GfxBuffer const& GetBuffer() const { return *result_buffer; }
		GfxBuffer const& operator*() const { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::unique_ptr<GfxBuffer> instance_buffer;
		void* instance_buffer_cpu_address = nullptr;
	};
}