#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_GI_Util.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct ReSTIR_GI_OutputConstants
{
    uint depthIdx;
    uint normalIdx;
    uint albedoIdx;
    uint finalReservoirIdx;
    uint outputIdx;
};
DECLARE_CBUFFER(ReSTIR_GI_OutputConstants, ReSTIR_GI_OutputCB, 1);

[numthreads(16, 16, 1)]
void ReSTIR_GI_OutputCS(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid.xy >= (uint2)FrameCB.renderResolution))
    {
        return;
    }

    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[ReSTIR_GI_OutputCB.outputIdx];
    Surface surface = GetSurface(DTid.xy, ReSTIR_GI_OutputCB.albedoIdx, ReSTIR_GI_OutputCB.normalIdx, ReSTIR_GI_OutputCB.depthIdx);
    if (surface.depth == 0.0f)
    {
        outputTexture[DTid.xy] = float4(0, 0, 0, 0);
        return;
    }

    ReSTIR_GI_Reservoir reservoir = ReSTIR_GI_LoadReservoir(DTid.xy, ReSTIR_GI_OutputCB.finalReservoirIdx);
    if (!ReSTIR_GI_IsValid(reservoir))
    {
        outputTexture[DTid.xy] = float4(0, 0, 0, 0);
        return;
    }

    float3 toSample = reservoir.position - surface.worldPos;
    float distance = length(toSample);
    if (distance > 1e-4f)
    {
        RayDesc visibilityRay;
        visibilityRay.Origin = OffsetRay(surface.worldPos, surface.worldNormal);
        visibilityRay.Direction = toSample / distance;
        visibilityRay.TMin = 1e-2f;
        visibilityRay.TMax = distance - 1e-2f;
        if (!TraceShadowRay(visibilityRay))
        {
            outputTexture[DTid.xy] = float4(0, 0, 0, 0);
            return;
        }
    }

    float3 indirect = ReSTIR_GI_EvaluateContribution(reservoir.position, reservoir.radiance, surface) * ReSTIR_GI_GetInvPdf(reservoir);

    if (any(isnan(indirect)) || any(isinf(indirect)))
    {
        indirect = 0.0f;
    }

    float luminance = CalculateLuminance(indirect);
    if (luminance > ReSTIR_GI_FireflyClamp)
    {
        indirect *= ReSTIR_GI_FireflyClamp / luminance;
    }

    outputTexture[DTid.xy] = float4(indirect, 1.0f);
}
