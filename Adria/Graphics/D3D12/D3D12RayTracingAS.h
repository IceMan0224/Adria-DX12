#pragma once
#include <d3d12.h>
#include "Graphics/GfxRayTracingAS.h"

namespace adria
{
	class D3D12CommandList;

	class D3D12RayTracingBLAS : public GfxRayTracingBLAS
	{
		friend class D3D12CommandList;
	public:
		D3D12RayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
		virtual ~D3D12RayTracingBLAS() override;

		virtual Uint64 GetGpuAddress() const override;
		virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geo_descs;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
	};

	class D3D12RayTracingTLAS : public GfxRayTracingTLAS
	{
		friend class D3D12CommandList;
	public:
		D3D12RayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
		virtual ~D3D12RayTracingTLAS() override;

		virtual Uint64 GetGpuAddress() const override;
		virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::unique_ptr<GfxBuffer> instance_buffer;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
	};
}