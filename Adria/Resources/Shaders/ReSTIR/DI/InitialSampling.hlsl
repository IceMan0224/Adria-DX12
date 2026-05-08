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
        ReSTIR_DI_Reservoir emptyReservoir = (ReSTIR_DI_Reservoir)0;
        ReSTIR_DI_StoreReservoir(emptyReservoir, DTid.xy, IntialSamplingCB.reservoirBufferIdx);
        return;
    }
    RNG rng = RNG_Initialize(DTid.x + DTid.y * (uint)FrameCB.renderResolution.x, FrameCB.frameCount, 16);
    
    LightSample lightSample = EmptyLightSample();
    ReSTIR_DI_Reservoir finalReservoir = ReSTIR_DI_SampleLightsForSurface(rng, surface, lightSample);

    // Initial visibility check: discard occluded samples early so temporal/spatial
    // resampling doesn't waste budget propagating invisible light samples
    uint selectedLight = ReSTIR_DI_GetLightIndex(finalReservoir);
    if (selectedLight != ReSTIR_InvalidLightIndex && selectedLight < (uint)FrameCB.lightCount)
    {
        LightInfo lightInfo = LoadLightInfo(selectedLight);
        if (lightInfo.shadowMaskIndex >= 0 && !TraceShadowRay(lightInfo, surface.worldPos, FrameCB.inverseView))
        {
            finalReservoir = ReSTIR_DI_EmptyDIReservoir();
        }
    }

    ReSTIR_DI_StoreReservoir(finalReservoir, DTid.xy, IntialSamplingCB.reservoirBufferIdx);
}
