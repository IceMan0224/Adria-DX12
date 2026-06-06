#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_GI_Util.hlsli"
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
    float maxSpatialM;
};

DECLARE_CBUFFER(SpatialResamplingConstants, SpatialResamplingCB, 1);

static const float NORMAL_THRESHOLD = cos(25.0f * 3.14159265f / 180.0f);
static const float DEPTH_THRESHOLD = 0.1f;

[numthreads(16, 16, 1)]
void SpatialResamplingCS(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid.xy >= (uint2)FrameCB.renderResolution))
    {
        return;
    }

    Surface surface = GetSurface(DTid.xy, SpatialResamplingCB.albedoIdx, SpatialResamplingCB.normalIdx, SpatialResamplingCB.depthIdx);
    if (surface.depth == 0.0f)
    {
        ReSTIR_GI_StoreReservoir(ReSTIR_GI_EmptyReservoir(), DTid.xy, SpatialResamplingCB.outputReservoirIdx);
        return;
    }

    ReSTIR_GI_Reservoir currentReservoir = ReSTIR_GI_LoadReservoir(DTid.xy, SpatialResamplingCB.inputReservoirIdx);
    RNG rng = RNG_Initialize(DTid.x + DTid.y * (uint)FrameCB.renderResolution.x, FrameCB.frameCount * 3 + 2, 16);
    ReSTIR_GI_Reservoir combined = ReSTIR_GI_EmptyReservoir();

    ReSTIR_GI_CombineReservoirs(combined, currentReservoir, RNG_GetNext(rng), currentReservoir.targetPdf);
    float centerDepth = surface.viewDepth;

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

        ReSTIR_GI_Reservoir neighborReservoir = ReSTIR_GI_LoadReservoir((uint2)neighborPixel, SpatialResamplingCB.inputReservoirIdx);
        if (!ReSTIR_GI_IsValid(neighborReservoir))
        {
            continue;
        }

        float jacobian = ReSTIR_GI_GetReconnectionJacobian(surface.worldPos, neighborSurface.worldPos, neighborReservoir);
        if (jacobian <= 0.0f || jacobian > ReSTIR_GI_MaxJacobian || jacobian < 1.0f / ReSTIR_GI_MaxJacobian)
        {
            continue;
        }
        jacobian = clamp(jacobian, 1.0f / 3.0f, 3.0f);

        float neighborTargetPdf = ReSTIR_GI_GetSampleTargetPdfForSurface(neighborReservoir.position, neighborReservoir.radiance, surface);
        neighborReservoir.weightSum *= jacobian;
        ReSTIR_GI_CombineReservoirs(combined, neighborReservoir, RNG_GetNext(rng), neighborTargetPdf);
    }

    ReSTIR_GI_FinalizeResampling(combined, 1.0, combined.M);
    combined.M = min(combined.M, SpatialResamplingCB.maxSpatialM);
    ReSTIR_GI_StoreReservoir(combined, DTid.xy, SpatialResamplingCB.outputReservoirIdx);
}
