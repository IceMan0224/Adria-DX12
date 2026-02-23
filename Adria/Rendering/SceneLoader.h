#pragma once
#include "Components.h"
#include "Meshlet.h"
#include "Math/NormalsUtil.h"
#include "Utilities/Heightmap.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	enum class LightMesh
	{
		NoMesh,
		Quad,
		Sphere
	};

    struct ModelParameters
    {
        std::string model_path = "";
        std::string textures_path = "";
		Matrix model_matrix;
		Bool triangle_ccw = true;
		Bool force_mask_alpha_usage = false;
		Bool load_model_lights = false;
    };
    struct SkyboxParameters
    {
        std::optional<std::string> cubemap;
        std::array<std::string, 6> cubemap_textures;
    };
	struct GridParameters
	{
		Uint64 tile_count_x;
		Uint64 tile_count_z;
		Float tile_size_x;
		Float tile_size_z;
		Float texture_scale_x;
		Float texture_scale_z;
		Uint64 chunk_count_x;
		Uint64 chunk_count_z;
		Bool split_to_chunks = false;
		NormalCalculation normal_type = NormalCalculation::None;
		std::unique_ptr<Heightmap> heightmap;
	};
	struct OceanParameters
	{
		GridParameters ocean_grid;
	};
	struct TerrainParameters
	{
		Float terrain_width  = 1024.0f;
		Float terrain_depth  = 1024.0f;
		Float height_scale   = 100.0f;

		Bool use_procedural = true;
		HeightmapDesc procedural_desc{
			.width = 513,
			.depth = 513,
			.max_height = 1,
			.fractal_type = FractalType::FBM,
			.noise_type = NoiseType::Perlin,
			.seed = 1337,
			.persistence = 0.45f,
			.lacunarity = 2.0f,
			.octaves = 6,
			.noise_scale = 500.0f
		};
		std::string heightmap_path;

		std::string splatmap_path;
		std::string layer_albedo_paths[4];
		std::string layer_normal_paths[4];
		std::string layer_arm_paths[4];
		Float layer_tiling[4] = { 32.0f, 32.0f, 32.0f, 32.0f };
	};
    struct LightParameters
    {
        Light light_data;
        LightMesh mesh_type = LightMesh::NoMesh;
        Uint32 mesh_size = 0u;
        std::optional<std::string> light_texture = std::nullopt;
    };
	struct DecalParameters
	{
		std::string name = "Decal";
		std::string albedo_texture_path;
		std::string normal_texture_path;
		Float rotation = 0.0f;
		Float size = 50.0f;
		Bool modify_gbuffer_normals = false;
		Vector3 position;
		Vector3 normal;
	};

	struct MeshData
	{
		DirectX::BoundingBox bounding_box;
		Int32 material_index = -1;
		GfxPrimitiveTopology topology = GfxPrimitiveTopology::TriangleList;

		std::vector<Vector3>		 positions_stream;
		std::vector<Vector3>		 normals_stream;
		std::vector<Vector4>		 tangents_stream;
		std::vector<Vector2>		 uvs_stream;
		std::vector<Uint32>			 indices;

		std::vector<Meshlet>		 meshlets;
		std::vector<Uint32>			 meshlet_vertices;
		std::vector<MeshletTriangle> meshlet_triangles;
	};

    class GfxDevice;
 
	class SceneLoader
	{
	public:
        
        SceneLoader(entt::registry& reg, GfxDevice* device);
		~SceneLoader();

		ADRIA_MAYBE_UNUSED entt::entity LoadSkybox(SkyboxParameters const&);
        ADRIA_MAYBE_UNUSED entt::entity LoadLight(LightParameters const&);
		ADRIA_MAYBE_UNUSED std::vector<entt::entity> LoadOcean(OceanParameters const&);
		ADRIA_MAYBE_UNUSED entt::entity LoadTerrain(TerrainParameters const&);
		ADRIA_MAYBE_UNUSED entt::entity LoadDecal(DecalParameters const&);
		ADRIA_MAYBE_UNUSED entt::entity LoadModel(ModelParameters const&);
	private:
        entt::registry& reg;
        GfxDevice* gfx;

	private:
		ADRIA_NODISCARD std::vector<entt::entity> LoadGrid(GridParameters const&);
		ADRIA_MAYBE_UNUSED entt::entity LoadModel_GLTF(ModelParameters const&);
		ADRIA_MAYBE_UNUSED entt::entity LoadModel_OBJ(ModelParameters const&);
		ADRIA_NODISCARD Uint64 CalculateTotalBufferSize(std::vector<MeshData>& mesh_data);
	};
}

