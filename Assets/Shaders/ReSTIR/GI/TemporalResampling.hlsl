#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_GI_Util.hlsli"
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
    float maxReservoirAge;
};

DECLARE_CBUFFER(TemporalResamplingConstants, TemporalResamplingCB, 1);

[numthreads(16, 16, 1)]
void TemporalResamplingCS(uint3 DTid : SV_DispatchThreadID)
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

    ReSTIR_GI_Reservoir currentReservoir = ReSTIR_GI_LoadReservoirRW(DTid.xy, TemporalResamplingCB.reservoirIdx);

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

    RNG rng = RNG_Initialize(DTid.x + DTid.y * (uint)FrameCB.renderResolution.x, FrameCB.frameCount * 3 + 1, 16);

    ReSTIR_GI_Reservoir prevReservoir = ReSTIR_GI_LoadReservoir((uint2)prevPixel, TemporalResamplingCB.prevReservoirIdx);
    float maxReservoirAge = TemporalResamplingCB.maxReservoirAge * (0.5f + RNG_GetNext(rng) * 0.5f);
    if (prevReservoir.age > maxReservoirAge)
    {
        prevReservoir = ReSTIR_GI_EmptyReservoir();
    }
    prevReservoir.M = min(prevReservoir.M, TemporalResamplingCB.maxTemporalM);

    float prevTargetPdf = ReSTIR_GI_GetSampleTargetPdfForSurface(prevReservoir.position, prevReservoir.radiance, surface);

    ReSTIR_GI_Reservoir combined = ReSTIR_GI_EmptyReservoir();

    ReSTIR_GI_CombineReservoirs(combined, currentReservoir, RNG_GetNext(rng), currentReservoir.targetPdf);
    bool selectedPrev = ReSTIR_GI_CombineReservoirs(combined, prevReservoir, RNG_GetNext(rng), prevTargetPdf);
    ReSTIR_GI_FinalizeResampling(combined, 1.0, combined.M);
    combined.age = selectedPrev ? (prevReservoir.age + 1.0f) : 0.0f;

    ReSTIR_GI_StoreReservoir(combined, DTid.xy, TemporalResamplingCB.reservoirIdx);
}
