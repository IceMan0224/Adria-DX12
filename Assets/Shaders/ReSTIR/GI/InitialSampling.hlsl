#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_GI_Util.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct InitialSamplingConstants
{
    uint depthIdx;
    uint normalIdx;
    uint albedoIdx;
    uint reservoirBufferIdx;
};
DECLARE_CBUFFER(InitialSamplingConstants, InitialSamplingCB, 1);

// Direct lighting at the secondary surface using world-space lights, selected with RIS and validated with one shadow ray.
float3 ReSTIR_GI_ShadeSecondarySurface(Surface surface, inout RNG rng)
{
    uint lightCount = (uint)FrameCB.lightCount;
    if (lightCount == 0)
    {
        return 0.0f;
    }

    uint candidateCount = min(8u, lightCount);
    float weightSum = 0.0f;
    uint selectedLight = ReSTIR_InvalidLightIndex;
    float2 selectedUV = 0.0f;
    LightInfo selectedInfoWS = (LightInfo)0;
    float selectedTargetPdf = 0.0f;

    for (uint i = 0; i < candidateCount; ++i)
    {
        uint lightIndex = min((uint)(RNG_GetNext(rng) * lightCount), lightCount - 1);
        LightInfo lightInfoWS = ReSTIR_LoadLightInfoWS(lightIndex);
        if (!lightInfoWS.active)
        {
            continue;
        }

        float2 uv = float2(RNG_GetNext(rng), RNG_GetNext(rng));
        LightSample candidateSample = ReSTIR_SampleLight(lightInfoWS, surface, uv);
        float targetPdf = ReSTIR_GetLightSampleTargetPdfForSurface(candidateSample, lightInfoWS, surface);
        float risWeight = targetPdf * lightCount;

        weightSum += risWeight;
        if (RNG_GetNext(rng) * weightSum < risWeight)
        {
            selectedLight = lightIndex;
            selectedUV = uv;
            selectedInfoWS = lightInfoWS;
            selectedTargetPdf = targetPdf;
        }
    }

    if (selectedLight == ReSTIR_InvalidLightIndex || selectedTargetPdf <= 0.0f || weightSum <= 0.0f)
    {
        return 0.0f;
    }
    if (!TraceShadowRay(selectedInfoWS, surface.worldPos))
    {
        return 0.0f;
    }

    LightSample lightSample = ReSTIR_SampleLight(selectedInfoWS, surface, selectedUV);
    float3 shaded = ReSTIR_ShadeSurfaceWithLightSample(lightSample, selectedInfoWS, surface);
    float invPdf = weightSum / (candidateCount * selectedTargetPdf);
    return shaded * invPdf;
}

[numthreads(16, 16, 1)]
void InitialSamplingCS(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid.xy >= (uint2)FrameCB.renderResolution))
    {
        return;
    }

    Surface surface = GetSurface(DTid.xy, InitialSamplingCB.albedoIdx, InitialSamplingCB.normalIdx, InitialSamplingCB.depthIdx);
    if (surface.depth == 0.0f)
    {
        ReSTIR_GI_StoreReservoir(ReSTIR_GI_EmptyReservoir(), DTid.xy, InitialSamplingCB.reservoirBufferIdx);
        return;
    }

    RNG rng = RNG_Initialize(DTid.x + DTid.y * (uint)FrameCB.renderResolution.x, FrameCB.frameCount, 16);

    float3 V = surface.viewDir;
    float3 N = surface.worldNormal;

    // Sample a BRDF bounce direction and record its solid-angle PDF.
    float3 bounceDir;
    if (RNG_GetNext(rng) < surface.diffuseProbability)
    {
        bounceDir = GetCosHemisphereSample(rng, N);
    }
    else
    {
        float2 Xi = float2(RNG_GetNext(rng), RNG_GetNext(rng));
        float4 hPdf = ImportanceSampleGGX(Xi, max(surface.brdfData.Roughness, MIN_ROUGHNESS));
        float3 H = TangentToWorld(hPdf.xyz, N);
        bounceDir = reflect(-V, H);
    }

    float NdotL = saturate(dot(N, bounceDir));
    if (NdotL <= 0.0f)
    {
        ReSTIR_GI_StoreReservoir(ReSTIR_GI_EmptyReservoir(), DTid.xy, InitialSamplingCB.reservoirBufferIdx);
        return;
    }

    float diffPdf = NdotL / PI;
    float3 H = normalize(V + bounceDir);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    float a = max(surface.brdfData.Roughness, MIN_ROUGHNESS) * max(surface.brdfData.Roughness, MIN_ROUGHNESS);
    float D = D_GGX(N, H, a);
    float specPdf = D * NdotH / max(4.0f * VdotH, 0.001f);
    float sourcePdf = surface.diffuseProbability * diffPdf + (1.0f - surface.diffuseProbability) * specPdf;
    if (sourcePdf < 1e-6f)
    {
        ReSTIR_GI_StoreReservoir(ReSTIR_GI_EmptyReservoir(), DTid.xy, InitialSamplingCB.reservoirBufferIdx);
        return;
    }

    RayDesc ray;
    ray.Origin = OffsetRay(surface.worldPos, N);
    ray.Direction = bounceDir;
    ray.TMin = 1e-2f;
    ray.TMax = FLT_MAX;

    float3 samplePos;
    float3 sampleNormal;
    float3 sampleRadiance;

    HitInfo hit = (HitInfo)0;
    if (TraceRay(ray, hit))
    {
        Instance instanceData = GetInstanceData(hit.instanceIndex);
        Mesh meshData = GetMeshData(instanceData.meshIndex);
        Material materialData = GetMaterialData(instanceData.materialIdx);
        VertexDataEx vert = LoadVertexDataEx(meshData, hit.primitiveIndex, hit.barycentricCoordinates);

        float3 worldPosition = mul(vert.pos, hit.objectToWorldMatrix).xyz;
        float3 worldNormal = normalize(mul(vert.nor, (float3x3)transpose(hit.worldToObjectMatrix)));

        MaterialProperties matProps = GetMaterialProperties(materialData, vert.uv, 0);
        if (materialData.normalIdx >= 0)
        {
            float3 rawTangent = mul(vert.tan.xyz, (float3x3)hit.objectToWorldMatrix);
            float tangentLenSq = dot(rawTangent, rawTangent);
            if (tangentLenSq > 1e-6f)
            {
                float3 worldTangent = rawTangent * rsqrt(tangentLenSq);
                float3 worldBitangent = cross(worldNormal, worldTangent) * sign(vert.tan.w);
                float3 normalTS = matProps.normalTS * 2.0f - 1.0f;
                worldNormal = normalize(normalTS.x * worldTangent + normalTS.y * worldBitangent + normalTS.z * worldNormal);
            }
        }

        Surface secondarySurface = GetEmptySurface();
        secondarySurface.worldPos = worldPosition;
        secondarySurface.worldNormal = worldNormal;
        secondarySurface.viewDir = -bounceDir;
        secondarySurface.brdfData = GetBrdfData(matProps);
        secondarySurface.diffuseProbability = GetSurfaceDiffuseProbability(secondarySurface);

        float3 directRadiance = ReSTIR_GI_ShadeSecondarySurface(secondarySurface, rng);

        samplePos = worldPosition;
        sampleNormal = worldNormal;
        sampleRadiance = directRadiance + matProps.emissive;
    }
    else
    {
        TextureCube envMapTexture = ResourceDescriptorHeap[FrameCB.envMapIdx];
        float3 envRadiance = envMapTexture.SampleLevel(LinearWrapSampler, bounceDir, 0).rgb;

        samplePos = surface.worldPos + bounceDir * 1e4f;
        sampleNormal = -bounceDir;
        sampleRadiance = envRadiance;
    }

    if (any(isnan(sampleRadiance)) || any(isinf(sampleRadiance)) ||
        any(isnan(sampleNormal))   || any(isinf(sampleNormal))   ||
        any(isnan(samplePos))      || any(isinf(samplePos)))
    {
        ReSTIR_GI_StoreReservoir(ReSTIR_GI_EmptyReservoir(), DTid.xy, InitialSamplingCB.reservoirBufferIdx);
        return;
    }

    float radianceLuminance = CalculateLuminance(sampleRadiance);
    if (radianceLuminance > ReSTIR_GI_MaxSampleRadiance)
    {
        sampleRadiance *= ReSTIR_GI_MaxSampleRadiance / radianceLuminance;
    }

    float targetPdf = ReSTIR_GI_GetSampleTargetPdfForSurface(samplePos, sampleRadiance, surface);

    ReSTIR_GI_Reservoir reservoir = ReSTIR_GI_EmptyReservoir();
    ReSTIR_GI_StreamSample(reservoir, samplePos, sampleNormal, sampleRadiance, RNG_GetNext(rng), targetPdf, 1.0f / sourcePdf);
    ReSTIR_GI_FinalizeResampling(reservoir, 1.0f, reservoir.M);
    reservoir.M = (reservoir.targetPdf > 0.0f) ? 1.0f : 0.0f;
    reservoir.age = 0.0f;

    ReSTIR_GI_StoreReservoir(reservoir, DTid.xy, InitialSamplingCB.reservoirBufferIdx);
}
