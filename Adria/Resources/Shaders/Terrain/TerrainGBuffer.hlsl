#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Terrain/TerrainCommon.hlsli"

#define NUM_CONTROL_POINTS 3
#define ShadingExtension_Default 0

struct TerrainConstants
{
	float  terrainWidth;
	float  terrainDepth;
	float  heightScale;
	float  minTessDistance;
	float  maxTessDistance;
	float  minTessFactor;
	float  maxTessFactor;
	uint   heightmapIdx;
	uint   normalmapIdx;
	uint   splatmapIdx;
	uint   layerAlbedoIdx[4];
	uint   layerNormalIdx[4];
	uint   layerArmIdx[4];
	float  layerTiling[4];
};
ConstantBuffer<TerrainConstants> TerrainCB : register(b2);

struct TerrainPatchConstants
{
	float2 patchWorldPos;
	float  patchSize;
	uint   pad0;
};
ConstantBuffer<TerrainPatchConstants> TerrainPatchCB : register(b3);

struct VSInput
{
	float2 PosXZ : POSITION;
};

struct VSToHS
{
	float3 WorldPos : POSITION;
	float2 TerrainUV : TEXCOORD0;
};

struct HSToDS
{
	float3 WorldPos : POSITION;
	float2 TerrainUV : TEXCOORD0;
};

struct HSConstantDataOutput
{
	float EdgeTessFactor[3] : SV_TessFactor;
	float InsideTessFactor  : SV_InsideTessFactor;
};

struct DSToPS
{
	float4 Position  : SV_POSITION;
	float3 WorldPos  : POSITION;
	float2 TerrainUV : TEXCOORD0;
};

struct PSOutput
{
	float4 NormalRT   : SV_TARGET0;
	float4 DiffuseRT  : SV_TARGET1;
	float4 EmissiveRT : SV_TARGET2;
	float4 CustomRT   : SV_TARGET3;
};

DSToPS TerrainVS(VSInput input)
{
	DSToPS output = (DSToPS)0;

	float2 worldXZ = TerrainPatchCB.patchWorldPos + input.PosXZ * TerrainPatchCB.patchSize;
	float2 terrainUV = WorldToTerrainUV(worldXZ, TerrainCB.terrainWidth, TerrainCB.terrainDepth);
	float height = SampleTerrainHeight(terrainUV, TerrainCB.heightmapIdx, TerrainCB.heightScale);

	output.WorldPos = float3(worldXZ.x, height, worldXZ.y);
	output.TerrainUV = terrainUV;
	output.Position = mul(float4(output.WorldPos, 1.0), FrameCB.viewProjection);
	output.Position.xy += FrameCB.cameraJitter * output.Position.w;
	return output;
}

VSToHS TerrainVSLOD(VSInput input)
{
	VSToHS output = (VSToHS)0;

	float2 worldXZ = TerrainPatchCB.patchWorldPos + input.PosXZ * TerrainPatchCB.patchSize;
	float2 terrainUV = WorldToTerrainUV(worldXZ, TerrainCB.terrainWidth, TerrainCB.terrainDepth);
	float height = SampleTerrainHeight(terrainUV, TerrainCB.heightmapIdx, TerrainCB.heightScale);

	output.WorldPos = float3(worldXZ.x, height, worldXZ.y);
	output.TerrainUV = terrainUV;
	return output;
}

float CalcTerrainTessFactor(float3 p)
{
	float d = distance(p, FrameCB.cameraPosition);
	float s = saturate((d - TerrainCB.minTessDistance) / (TerrainCB.maxTessDistance - TerrainCB.minTessDistance));
	return lerp(TerrainCB.maxTessFactor, TerrainCB.minTessFactor, s);
}

HSConstantDataOutput CalcTerrainHSLODPatchConstants(
	InputPatch<VSToHS, NUM_CONTROL_POINTS> inputPatch,
	uint patchId : SV_PrimitiveID)
{
	HSConstantDataOutput output;

	float3 e0 = 0.5 * (inputPatch[1].WorldPos + inputPatch[2].WorldPos);
	float3 e1 = 0.5 * (inputPatch[2].WorldPos + inputPatch[0].WorldPos);
	float3 e2 = 0.5 * (inputPatch[0].WorldPos + inputPatch[1].WorldPos);
	float3 center = (inputPatch[0].WorldPos + inputPatch[1].WorldPos + inputPatch[2].WorldPos) / 3.0;

	output.EdgeTessFactor[0] = CalcTerrainTessFactor(e0);
	output.EdgeTessFactor[1] = CalcTerrainTessFactor(e1);
	output.EdgeTessFactor[2] = CalcTerrainTessFactor(e2);
	output.InsideTessFactor  = CalcTerrainTessFactor(center);

	return output;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcTerrainHSLODPatchConstants")]
HSToDS TerrainHSLOD(
	InputPatch<VSToHS, NUM_CONTROL_POINTS> inputPatch,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	HSToDS output;
	output.WorldPos = inputPatch[i].WorldPos;
	output.TerrainUV = inputPatch[i].TerrainUV;
	return output;
}

[domain("tri")]
DSToPS TerrainDSLOD(
	HSConstantDataOutput input,
	float3 domain : SV_DomainLocation,
	const OutputPatch<HSToDS, NUM_CONTROL_POINTS> patch)
{
	DSToPS output;

	float2 terrainUV = domain.x * patch[0].TerrainUV +
					   domain.y * patch[1].TerrainUV +
					   domain.z * patch[2].TerrainUV;

	float2 worldXZ = domain.x * patch[0].WorldPos.xz +
					 domain.y * patch[1].WorldPos.xz +
					 domain.z * patch[2].WorldPos.xz;

	float height = SampleTerrainHeight(terrainUV, TerrainCB.heightmapIdx, TerrainCB.heightScale);

	output.WorldPos = float3(worldXZ.x, height, worldXZ.y);
	output.TerrainUV = terrainUV;
	output.Position = mul(float4(output.WorldPos, 1.0), FrameCB.viewProjection);
	output.Position.xy += FrameCB.cameraJitter * output.Position.w;
	return output;
}

PSOutput TerrainPS(DSToPS input)
{
	PSOutput output = (PSOutput)0;
	float3 terrainNormal = SampleTerrainNormal(input.TerrainUV, TerrainCB.normalmapIdx);

	Texture2D splatmapTex = ResourceDescriptorHeap[TerrainCB.splatmapIdx];
	float4 splatWeights = splatmapTex.Sample(LinearClampSampler, input.TerrainUV);
	float weightSum = dot(splatWeights, 1.0);
	if (weightSum > 0.001) splatWeights /= weightSum;
	else splatWeights = float4(1, 0, 0, 0);

	float3 albedo = 0;
	float roughness = 0;
	float metallic = 0;
	float3 blendedNormal = 0;

	[unroll]
	for (uint i = 0; i < 4; ++i)
	{
		float w = splatWeights[i];
		if (w < 0.001) continue;

		float2 layerUV = input.TerrainUV * TerrainCB.layerTiling[i];

		Texture2D albedoTex = ResourceDescriptorHeap[TerrainCB.layerAlbedoIdx[i]];
		Texture2D normalTex = ResourceDescriptorHeap[TerrainCB.layerNormalIdx[i]];
		Texture2D armTex    = ResourceDescriptorHeap[TerrainCB.layerArmIdx[i]];

		albedo    += w * albedoTex.Sample(LinearWrapSampler, layerUV).rgb;
		float3 arm = armTex.Sample(LinearWrapSampler, layerUV).rgb;
		roughness += w * arm.g;
		metallic  += w * arm.b;

		float3 nTS = normalTex.Sample(LinearWrapSampler, layerUV).xyz * 2.0 - 1.0;
		blendedNormal += w * nTS;
	}

	blendedNormal = normalize(blendedNormal);

	float3 N = normalize(terrainNormal);
	float3 up = abs(N.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
	float3 T = normalize(cross(up, N));
	float3 B = cross(N, T);
	float3x3 TBN = float3x3(T, B, N);

	float3 worldNormal = normalize(mul(blendedNormal, TBN));
	float3 viewNormal = normalize(mul(worldNormal, (float3x3)FrameCB.view));

	output.NormalRT = EncodeGBufferNormalRT(viewNormal, metallic, ShadingExtension_Default);
	output.DiffuseRT = float4(albedo, roughness);
	output.EmissiveRT = 0;
	output.CustomRT = 0;
	return output;
}
