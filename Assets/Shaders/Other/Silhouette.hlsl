#include "CommonResources.hlsli"

#define MAX_SILHOUETTE_IDS 128
#define MAX_SILHOUETTE_PACKED ((MAX_SILHOUETTE_IDS + 3) / 4)

struct SilhouetteConstants
{
    float outlineWidth;
    float outlineR;
    float outlineG;
    float outlineB;
    uint  hdrIdx;
    uint  entityIdIdx;
    uint  outputIdx;
    uint  _pad;
};
DECLARE_CBUFFER(SilhouetteConstants, SilhouetteCB, 1);

struct SilhouetteIds
{
    uint  idCount;
    uint  _pad0;
    uint  _pad1;
    uint  _pad2;
    uint4 ids[MAX_SILHOUETTE_PACKED];
};
DECLARE_CBUFFER(SilhouetteIds, SilhouetteIdsCB, 2);

static bool IsSelected(uint id)
{
    if (id == 0xffffffffu)
    {
        return false;
    }
    uint const count = SilhouetteIdsCB.idCount;
    for (uint i = 0; i < count; ++i)
    {
        if (SilhouetteIdsCB.ids[i / 4][i % 4] == id)
        {
            return true;
        }
    }
    return false;
}

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

    if (SilhouetteIdsCB.idCount == 0)
    {
        outputTex[pixel] = sceneColor;
        return;
    }

    if (IsSelected(centerId))
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
            if (IsSelected(entityIdTex[coord]))
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
