#pragma once
#include <vector>
#include <memory>
#include <d3d12.h>
#include <DirectXMath.h>

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	struct Mesh;
	struct Transform;

	class AccelerationStructure
	{
		
	public:
		explicit AccelerationStructure(GfxDevice* gfx);

		void AddInstance(Mesh const& submesh, Transform const& transform, bool is_transparent = false);
		void Build(); //add build flag options

		GfxBuffer const* GetTLAS() const;

	private:
		GfxDevice* gfx;
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geo_descs;
		std::vector<DirectX::XMMATRIX> geo_transforms;

		std::vector<std::unique_ptr<GfxBuffer>> blases;
		std::unique_ptr<GfxBuffer> tlas;
	private:
		void BuildBottomLevels();
		void BuildTopLevel();
	};
}