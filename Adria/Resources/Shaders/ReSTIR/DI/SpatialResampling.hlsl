#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_DI_Util.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct SpatialResamplingConstants
{
	uint depthIdx;
	uint normalIdx;
	uint albedoIdx;
	uint inputReservoirIdx;
	uint outputReservoirIdx;
	uint spatialSampleCount;
	float spatialRadius;
};

ConstantBuffer<SpatialResamplingConstants> SpatialResamplingCB : register(b1);

static const float NORMAL_THRESHOLD = cos(25.0f * 3.14159265f / 180.0f); 
static const float DEPTH_THRESHOLD = 0.1f; 

[numthreads(16, 16, 1)]
void SpatialResamplingCS( uint3 DTid : SV_DispatchThreadID )
{
	if (any(DTid.xy >= (uint2)FrameCB.renderResolution)) 
	{
		return;
	}

	Surface surface = GetSurface(DTid.xy, SpatialResamplingCB.albedoIdx, SpatialResamplingCB.normalIdx, SpatialResamplingCB.depthIdx);
	if (surface.depth == 0.0f)
	{
		ReSTIR_DI_Reservoir emptyReservoir = ReSTIR_DI_EmptyDIReservoir();
		ReSTIR_DI_StoreReservoir(emptyReservoir, DTid.xy, SpatialResamplingCB.outputReservoirIdx);
		return;
	}

	ReSTIR_DI_Reservoir currentReservoir = ReSTIR_DI_LoadReservoir(DTid.xy, SpatialResamplingCB.inputReservoirIdx);
	RNG rng = RNG_Initialize(DTid.x + DTid.y * (uint)FrameCB.renderResolution.x, FrameCB.frameCount * 3 + 2, 16);
	ReSTIR_DI_Reservoir combined = ReSTIR_DI_EmptyDIReservoir();

	float currentTargetPdf = currentReservoir.targetPdf;
	uint currentLightIndex = ReSTIR_DI_GetLightIndex(currentReservoir);
	if (currentTargetPdf > 0.0f && currentLightIndex != ReSTIR_InvalidLightIndex && currentLightIndex < (uint)FrameCB.lightCount)
	{
		LightInfo currentLightInfo = LoadLightInfo(currentLightIndex);
		if (currentLightInfo.shadowMaskIndex >= 0
			&& !TraceShadowRay(currentLightInfo, surface.worldPos, FrameCB.inverseView))
		{
			currentTargetPdf = 0.0f;
		}
	}
	ReSTIR_DI_CombineReservoirs(combined, currentReservoir, RNG_GetNext(rng), currentTargetPdf);
	float centerDepth = surface.viewDepth;

	static const uint MAX_SPATIAL_SAMPLES = 16;
	int2 validNeighborPixels[MAX_SPATIAL_SAMPLES];
	float validNeighborMs[MAX_SPATIAL_SAMPLES];
	uint validNeighborCount = 0;

	for (uint i = 0; i < SpatialResamplingCB.spatialSampleCount; ++i)
	{
		float angle = RNG_GetNext(rng) * 2.0f * 3.14159265f;
		float radius = (RNG_GetNext(rng) + 0.1f) * SpatialResamplingCB.spatialRadius;
		int2 neighborPixel = (int2)DTid.xy + int2(cos(angle) * radius, sin(angle) * radius);

		if (any(neighborPixel < 0) || any(neighborPixel >= (int2)FrameCB.renderResolution))
		{
			continue;
		}

		Surface neighborSurface = GetSurface((uint2)neighborPixel, SpatialResamplingCB.albedoIdx, SpatialResamplingCB.normalIdx, SpatialResamplingCB.depthIdx);
		if (neighborSurface.depth == 0.0f)
		{
			continue;
		}

		float normalSimilarity = dot(surface.worldNormal, neighborSurface.worldNormal);
		if (normalSimilarity < NORMAL_THRESHOLD)
		{
			continue;
		}

		float neighborDepth = neighborSurface.viewDepth;
		float depthDiff = abs(centerDepth - neighborDepth) / max(centerDepth, 1e-6f);
		if (depthDiff > DEPTH_THRESHOLD)
		{
			continue;
		}

		ReSTIR_DI_Reservoir neighborReservoir = ReSTIR_DI_LoadReservoir((uint2)neighborPixel, SpatialResamplingCB.inputReservoirIdx);
		float neighborTargetPdf = 0.0f;
		uint neighborLightIndex = ReSTIR_DI_GetLightIndex(neighborReservoir);
		if (neighborLightIndex != ReSTIR_InvalidLightIndex && neighborLightIndex < (uint)FrameCB.lightCount)
		{
			LightInfo neighborLightInfo = LoadLightInfo(neighborLightIndex);
			float2 neighborSampleUV = ReSTIR_DI_GetSampleUV(neighborReservoir);
			LightSample neighborLightSample = ReSTIR_SampleLight(neighborLightInfo, surface, neighborSampleUV);
			neighborTargetPdf = ReSTIR_GetLightSampleTargetPdfForSurface(neighborLightSample, neighborLightInfo, surface);

			if (neighborTargetPdf > 0.0f && neighborLightInfo.shadowMaskIndex >= 0
				&& !TraceShadowRay(neighborLightInfo, surface.worldPos, FrameCB.inverseView))
			{
				neighborTargetPdf = 0.0f;
			}
		}

		validNeighborPixels[validNeighborCount] = neighborPixel;
		validNeighborMs[validNeighborCount] = neighborReservoir.M;
		validNeighborCount++;

		ReSTIR_DI_CombineReservoirs(combined, neighborReservoir, RNG_GetNext(rng), neighborTargetPdf);
	}

#ifdef RESTIR_DI_BIAS_CORRECTION_OFF
	ReSTIR_DI_FinalizeResampling(combined, 1.0, combined.M);
#else
	// basic bias correction from RTXDI: count M only from surfaces where selected sample has non-zero target PDF
	float selectedMDenom = 0.0f;
	uint selectedLight = ReSTIR_DI_GetLightIndex(combined);
	if (selectedLight != ReSTIR_InvalidLightIndex && selectedLight < (uint)FrameCB.lightCount)
	{
		LightInfo selectedLightInfo = LoadLightInfo(selectedLight);
		float2 selectedUV = ReSTIR_DI_GetSampleUV(combined);

		LightSample lsAtCenter = ReSTIR_SampleLight(selectedLightInfo, surface, selectedUV);
		if (ReSTIR_GetLightSampleTargetPdfForSurface(lsAtCenter, selectedLightInfo, surface) > 0.0f)
			selectedMDenom += currentReservoir.M;

		for (uint j = 0; j < validNeighborCount; ++j)
		{
			Surface nSurface = GetSurface((uint2)validNeighborPixels[j], SpatialResamplingCB.albedoIdx, SpatialResamplingCB.normalIdx, SpatialResamplingCB.depthIdx);
			LightSample lsAtNeighbor = ReSTIR_SampleLight(selectedLightInfo, nSurface, selectedUV);
			if (ReSTIR_GetLightSampleTargetPdfForSurface(lsAtNeighbor, selectedLightInfo, nSurface) > 0.0f)
				selectedMDenom += validNeighborMs[j];
		}
	}
	ReSTIR_DI_FinalizeResampling(combined, 1.0, max(selectedMDenom, 0.001f));
#endif

	ReSTIR_DI_StoreReservoir(combined, DTid.xy, SpatialResamplingCB.outputReservoirIdx);
}
