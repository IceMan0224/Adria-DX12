#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_DI_Util.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct ReSTIR_DI_OutputConstants
{
	uint depthIdx;
	uint normalIdx;
	uint albedoIdx;
	uint finalReservoirIdx;
	uint outputIdx;
};
DECLARE_CBUFFER(ReSTIR_DI_OutputConstants, ReSTIR_DI_OutputCB, 1);

[numthreads(16, 16, 1)]
void ReSTIR_DI_OutputCS(uint3 DTid : SV_DispatchThreadID)
{
	if (any(DTid.xy >= (uint2)FrameCB.renderResolution)) 
	{
		return;
	}

	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[ReSTIR_DI_OutputCB.outputIdx];
	Surface surface = GetSurface(DTid.xy, ReSTIR_DI_OutputCB.albedoIdx, ReSTIR_DI_OutputCB.normalIdx, ReSTIR_DI_OutputCB.depthIdx);
	if (surface.depth == 0.0f)
	{
		outputTexture[DTid.xy] = float4(0, 0, 0, 0);
		return;
	}

	ReSTIR_DI_Reservoir reservoir = ReSTIR_DI_LoadReservoir(DTid.xy, ReSTIR_DI_OutputCB.finalReservoirIdx);
	uint lightIndex = ReSTIR_DI_GetLightIndex(reservoir);
	if (reservoir.M == 0 || lightIndex == ReSTIR_InvalidLightIndex)
	{
		outputTexture[DTid.xy] = float4(0, 0, 0, 0);
		return;
	}

	LightInfo lightInfo = LoadLightInfo(lightIndex);
	if (lightInfo.shadowMaskIndex >= 0 && !TraceShadowRay(lightInfo, surface.worldPos, FrameCB.inverseView))
	{
		outputTexture[DTid.xy] = float4(0, 0, 0, 0);
		return;
	}

	float2 sampleUV = ReSTIR_DI_GetSampleUV(reservoir);
	LightSample lightSample = ReSTIR_SampleLight(lightInfo, surface, sampleUV);

	float3 shadingOutput = ReSTIR_ShadeSurfaceWithLightSample(lightSample, lightInfo, surface)
						 * ReSTIR_DI_GetInvPdf(reservoir);

	outputTexture[DTid.xy] = float4(shadingOutput, 1.0f);
}
