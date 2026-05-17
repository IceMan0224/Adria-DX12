#include "CommonResources.hlsli"

struct SilhouetteConstants
{
    uint  selectedEntityId;
    float outlineWidth;
    float outlineR;
    float outlineG;
    float outlineB;
    uint  hdrIdx;
    uint  entityIdIdx;
    uint  outputIdx;
};
DECLARE_CBUFFER(SilhouetteConstants, SilhouetteCB, 1);

[numthreads(8, 8, 1)]
void SilhouetteCS(uint3 dispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   hdrTexture  = ResourceDescriptorHeap[SilhouetteCB.hdrIdx];
    Texture2D<uint>     entityIdTex = ResourceDescriptorHeap[SilhouetteCB.entityIdIdx];
    RWTexture2D<float4> outputTex   = ResourceDescriptorHeap[SilhouetteCB.outputIdx];

    uint2 pixel = dispatchId.xy;
    uint2 dims;
    hdrTexture.GetDimensions(dims.x, dims.y);
    if (any(pixel >= dims)) 
    {
        return;
    }

    float4 sceneColor = hdrTexture[pixel];
    uint   centerId   = entityIdTex[pixel];
    
    if (centerId == SilhouetteCB.selectedEntityId)
    {
        outputTex[pixel] = sceneColor;
        return;
    }

    int radius = (int)SilhouetteCB.outlineWidth;
    bool onBorder = false;
    for (int dy = -radius; dy <= radius; dy++)
    {
        for (int dx = -radius; dx <= radius; dx++)
        {
            if (dx == 0 && dy == 0) continue;
            int2 coord = clamp((int2)pixel + int2(dx, dy), int2(0, 0), (int2)dims - 1);
            if (entityIdTex[coord] == SilhouetteCB.selectedEntityId)
            {
                onBorder = true;
                break;
            }
        }
        if (onBorder) 
        {
            break;
        }
    }

    outputTex[pixel] = onBorder
        ? float4(SilhouetteCB.outlineR, SilhouetteCB.outlineG, SilhouetteCB.outlineB, 1.0f)
        : sceneColor;
}
