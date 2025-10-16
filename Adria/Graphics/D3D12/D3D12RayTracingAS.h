#pragma once
#include "Graphics/GfxRayTracingAS.h"

namespace adria
{
	class D3D12RayTracingBLAS : public GfxRayTracingBLAS
	{
	public:
		D3D12RayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags);
		virtual ~D3D12RayTracingBLAS() override;

		virtual Uint64 GetGpuAddress() const override;
		virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
	};

	class D3D12RayTracingTLAS : public GfxRayTracingTLAS
	{
	public:
		D3D12RayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags);
		virtual ~D3D12RayTracingTLAS() override;

		virtual Uint64 GetGpuAddress() const override;
		virtual GfxBuffer const& GetBuffer() const override { return *result_buffer; }

	private:
		std::unique_ptr<GfxBuffer> result_buffer;
		std::unique_ptr<GfxBuffer> scratch_buffer;
		std::unique_ptr<GfxBuffer> instance_buffer;
		void* instance_buffer_cpu_address = nullptr;
	};
}