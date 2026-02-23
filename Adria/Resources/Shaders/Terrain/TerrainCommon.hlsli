#ifndef _TERRAIN_COMMON_
#define _TERRAIN_COMMON_

float2 WorldToTerrainUV(float2 worldXZ, float terrainWidth, float terrainDepth)
{
	return worldXZ / float2(terrainWidth, terrainDepth) + 0.5;
}

float SampleTerrainHeight(float2 terrainUV, uint heightmapIdx, float heightScale)
{
	Texture2D heightmap = ResourceDescriptorHeap[heightmapIdx];
	float h = heightmap.SampleLevel(LinearClampSampler, terrainUV, 0.0).r;
	return h * heightScale;
}

float3 SampleTerrainNormal(float2 terrainUV, uint normalmapIdx)
{
	Texture2D normalmap = ResourceDescriptorHeap[normalmapIdx];
	float3 n = normalmap.SampleLevel(LinearClampSampler, terrainUV, 0.0).xyz;
	return normalize(n * 2.0 - 1.0);
}

#endif
