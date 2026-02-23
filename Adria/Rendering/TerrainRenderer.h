#pragma once
#include "TextureHandle.h"
#include "Graphics/GfxDescriptor.h"
#include "Graphics/GfxPipelineStateFwd.h"
#include "RenderGraph/RenderGraphResourceId.h"
#include "Utilities/Heightmap.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxTexture;
	class GfxBuffer;

	struct TerrainQuadNode
	{
		Float x, z;
		Float size;
		Uint32 lod;
	};

	class TerrainRenderer
	{
		static constexpr Uint32 PATCH_GRID_SIZE = 32;
		static constexpr Uint32 MAX_LOD = 6;

	public:
		TerrainRenderer(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);
		~TerrainRenderer();

		void AddPasses(RenderGraph& rendergraph);
		void GUI();
		void OnResize(Uint32 w, Uint32 h);
		void OnSceneInitialized();

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		Uint32 width, height;

		std::unique_ptr<GfxTexture> heightmap_texture;
		GfxDescriptor heightmap_srv;
		Uint32 heightmap_width = 0;
		Uint32 heightmap_depth = 0;

		std::unique_ptr<GfxTexture> normalmap_texture;
		GfxDescriptor normalmap_srv;
		GfxDescriptor normalmap_uav;
		Bool normals_generated = false;

		std::unique_ptr<GfxTexture> splatmap_texture;
		GfxDescriptor splatmap_srv;

		std::unique_ptr<GfxBuffer> patch_vertex_buffer;
		std::unique_ptr<GfxBuffer> patch_index_buffer;
		Uint32 patch_index_count = 0;

		std::vector<TerrainQuadNode> visible_patches;

		std::unique_ptr<GfxGraphicsPipelineStatePermutations> terrain_psos;
		std::unique_ptr<GfxComputePipelineState> terrain_normals_pso;

		Bool wireframe = false;
		Bool gpu_resources_initialized = false;

	private:
		void CreatePSOs();
		void CreatePatchMesh();
		void InitializeGPUResources();
		void CreateHeightmapTexture(Heightmap const& hm);
		void CreateNormalmapTexture();
		void CreateSplatmapTexture(std::string const& path);
		void CreateDefaultSplatmap();
		void BuildQuadtree(Float cam_x, Float cam_z, Float terrain_width, Float terrain_depth);
		void SubdivideNode(Float cam_x, Float cam_z, Float node_x, Float node_z, Float node_size, Uint32 lod, Float terrain_half_w, Float terrain_half_d);
	};
}
