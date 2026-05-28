#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_DI_Util.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct IntialSamplingConstants
{
    uint depthIdx;
    uint normalIdx;
    uint albedoIdx;
    uint reservoirBufferIdx;
};
DECLARE_CBUFFER(IntialSamplingConstants, IntialSamplingCB, 1);

[numthreads(16, 16, 1)]
void InitialSamplingCS( uint3 DTid : SV_DispatchThreadID )
{
    if (any(DTid.xy >= (uint2)FrameCB.renderResolution)) 
    {
        return;
    }

    Surface surface = GetSurface(DTid.xy, IntialSamplingCB.albedoIdx, IntialSamplingCB.normalIdx, IntialSamplingCB.depthIdx);
    if (surface.depth == 0.0f)
    {
        ReSTIR_DI_StoreReservoir(ReSTIR_DI_EmptyDIReservoir(), DTid.xy, IntialSamplingCB.reservoirBufferIdx);
        return;
    }
    RNG rng = RNG_Initialize(DTid.x + DTid.y * (uint)FrameCB.renderResolution.x, FrameCB.frameCount, 16);

    LightSample lightSample = EmptyLightSample();
    ReSTIR_DI_Reservoir finalReservoir = ReSTIR_DI_SampleLightsForSurface(rng, surface, lightSample);

    uint selectedLight = ReSTIR_DI_GetLightIndex(finalReservoir);
    if (selectedLight != ReSTIR_InvalidLightIndex && selectedLight < (uint)FrameCB.lightCount)
    {
        LightInfo lightInfoWS = ReSTIR_LoadLightInfoWS(selectedLight);
        if (!TraceShadowRay(lightInfoWS, surface.worldPos))
        {
            finalReservoir = ReSTIR_DI_EmptyDIReservoir();
        }
    }

    ReSTIR_DI_StoreReservoir(finalReservoir, DTid.xy, IntialSamplingCB.reservoirBufferIdx);
}
