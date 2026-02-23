#include "CommonResources.hlsli"

struct TerrainNormalsConstants
{
	uint  heightmapIdx;
	uint  outputIdx;
	float texelSize;
	float heightScale;
};
ConstantBuffer<TerrainNormalsConstants> TerrainNormalsCB : register(b1);

[numthreads(16, 16, 1)]
void TerrainNormalsCS(uint3 id : SV_DispatchThreadID)
{
	Texture2D<float> heightmap = ResourceDescriptorHeap[TerrainNormalsCB.heightmapIdx];
	RWTexture2D<float4> normalMap = ResourceDescriptorHeap[TerrainNormalsCB.outputIdx];

	uint2 dims;
	heightmap.GetDimensions(dims.x, dims.y);
	if (id.x >= dims.x || id.y >= dims.y) return;

	float hL = heightmap[uint2(max(id.x, 1u) - 1, id.y)].r * TerrainNormalsCB.heightScale;
	float hR = heightmap[uint2(min(id.x + 1, dims.x - 1), id.y)].r * TerrainNormalsCB.heightScale;
	float hD = heightmap[uint2(id.x, max(id.y, 1u) - 1)].r * TerrainNormalsCB.heightScale;
	float hU = heightmap[uint2(id.x, min(id.y + 1, dims.y - 1))].r * TerrainNormalsCB.heightScale;

	float3 normal = normalize(float3(hR - hL, 2.0 * TerrainNormalsCB.texelSize, hU - hD));
	normalMap[id.xy] = float4(normal * 0.5 + 0.5, 0.0);
}
