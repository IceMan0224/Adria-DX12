#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_DI_Util.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct TemporalResamplingConstants
{
	uint depthIdx;
	uint normalIdx;
	uint albedoIdx;
	uint prevDepthIdx;
	uint reservoirIdx;
	uint prevReservoirIdx;
	float maxTemporalM;
	float depthThreshold;
	float normalThreshold;
};

DECLARE_CBUFFER(TemporalResamplingConstants, TemporalResamplingCB, 1);

[numthreads(16, 16, 1)]
void TemporalResamplingCS( uint3 DTid : SV_DispatchThreadID )
{
	if (any(DTid.xy >= (uint2)FrameCB.renderResolution))
	{
		return;
	}

	Surface surface = GetSurface(DTid.xy, TemporalResamplingCB.albedoIdx, TemporalResamplingCB.normalIdx, TemporalResamplingCB.depthIdx);
	if (surface.depth == 0.0f)
	{
		return;
	}

	ReSTIR_DI_Reservoir currentReservoir = ReSTIR_DI_LoadReservoirRW(DTid.xy, TemporalResamplingCB.reservoirIdx);

	float2 uv = ((float2)DTid.xy + 0.5) * rcp(FrameCB.renderResolution);
	float2 currentClip = uv * float2(2, -2) + float2(-1, 1);
	float4 previousClip = mul(float4(currentClip, surface.depth, 1.0f), FrameCB.reprojection);
	previousClip.xyz /= previousClip.w;
	float2 prevUV = previousClip.xy * float2(0.5, -0.5) + 0.5;
	int2 prevPixel = (int2)(prevUV * FrameCB.renderResolution);

	if (any(prevPixel < 0) || any(prevPixel >= (int2)FrameCB.renderResolution))
	{
		return;
	}

	Texture2D<float> prevDepthTexture = ResourceDescriptorHeap[TemporalResamplingCB.prevDepthIdx];
	float prevDepthRaw = prevDepthTexture[(uint2)prevPixel];
	if (prevDepthRaw == 0.0f)
	{
		return;
	}

	float prevViewDepth = LinearizeDepth(prevDepthRaw);
	float expectedPrevViewDepth = LinearizeDepth(previousClip.z);
	float depthDiff = abs(expectedPrevViewDepth - prevViewDepth) / max(expectedPrevViewDepth, 1e-6f);
	if (depthDiff > TemporalResamplingCB.depthThreshold)
	{
		return;
	}

	ReSTIR_DI_Reservoir prevReservoir = ReSTIR_DI_LoadReservoir((uint2)prevPixel, TemporalResamplingCB.prevReservoirIdx);
	prevReservoir.M = min(prevReservoir.M, TemporalResamplingCB.maxTemporalM);

	float prevTargetPdf = 0.0f;
	uint prevLightIndex = ReSTIR_DI_GetLightIndex(prevReservoir);
	if (prevLightIndex != ReSTIR_InvalidLightIndex && prevLightIndex < (uint)FrameCB.lightCount)
	{
		LightInfo prevLightInfoWS = ReSTIR_LoadLightInfoWS(prevLightIndex);
		if (prevLightInfoWS.active)
		{
			float2 prevSampleUV = ReSTIR_DI_GetSampleUV(prevReservoir);
			LightSample prevLightSample = ReSTIR_SampleLight(prevLightInfoWS, surface, prevSampleUV);
			prevTargetPdf = ReSTIR_GetLightSampleTargetPdfForSurface(prevLightSample, prevLightInfoWS, surface);
		}
	}

	RNG rng = RNG_Initialize(DTid.x + DTid.y * (uint)FrameCB.renderResolution.x, FrameCB.frameCount * 3 + 1, 16);
	ReSTIR_DI_Reservoir combined = ReSTIR_DI_EmptyDIReservoir();

	float currentTargetPdf = currentReservoir.targetPdf;
	ReSTIR_DI_CombineReservoirs(combined, currentReservoir, RNG_GetNext(rng), currentTargetPdf);
	ReSTIR_DI_CombineReservoirs(combined, prevReservoir, RNG_GetNext(rng), prevTargetPdf);
	ReSTIR_DI_FinalizeResampling(combined, 1.0, combined.M);

	ReSTIR_DI_StoreReservoir(combined, DTid.xy, TemporalResamplingCB.reservoirIdx);
}
