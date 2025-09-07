#include "PathTracing.hlsli"

struct PathTracingConstants
{
    int  bounceCount;
    int  accumulatedFrames;   
#if SVGF_ENABLED
    uint directRadianceIdx;
    uint indirectRadianceIdx;
    uint directAlbedoIdx;
    uint indirectAlbedoIdx;
#else 
    uint accumIdx;            
    uint outputIdx;   
#endif
};
ConstantBuffer<PathTracingConstants> PathTracingPassCB : register(b1);

[shader("raygeneration")]
void PT_RayGen()
{
    uint2 launchIdx = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;
    float2 resolution = float2(launchDim);

#if SVGF_ENABLED
    RWTexture2D<float4> directRadianceTex   = ResourceDescriptorHeap[PathTracingPassCB.directRadianceIdx];
    RWTexture2D<float4> indirectRadianceTex = ResourceDescriptorHeap[PathTracingPassCB.indirectRadianceIdx];
    RWTexture2D<float4> directAlbedoTex     = ResourceDescriptorHeap[PathTracingPassCB.directAlbedoIdx];
    RWTexture2D<float4> indirectAlbedoTex   = ResourceDescriptorHeap[PathTracingPassCB.indirectAlbedoIdx];
#else
    RWTexture2D<float4> accumulationTexture = ResourceDescriptorHeap[PathTracingPassCB.accumIdx];
    RWTexture2D<float4> outputTexture       = ResourceDescriptorHeap[PathTracingPassCB.outputIdx];
#endif

    uint seedBase = launchIdx.x + launchIdx.y * launchDim.x;
    RNG rng = RNG_Initialize(seedBase, FrameCB.frameCount, 16);
    float2 jitter = float2(RNG_GetNext(rng), RNG_GetNext(rng));

    float2 pixel = float2(launchIdx) + lerp(-0.5f.xx, 0.5f.xx, jitter);

    float2 ncdXY = (pixel / (resolution * 0.5f)) - 1.0f;
    ncdXY.y *= -1.0f;
    float4 rayStart = mul(float4(ncdXY, 1.0f, 1.0f), FrameCB.inverseViewProjection);
    float4 rayEnd   = mul(float4(ncdXY, 0.0f, 1.0f), FrameCB.inverseViewProjection);

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz   /= rayEnd.w;
    float3 rayDir = normalize(rayEnd.xyz - rayStart.xyz);

    RayDesc ray;
    ray.Origin = rayStart.xyz;
    ray.Direction = rayDir;
    ray.TMin = 0.0f;
    ray.TMax = FLT_MAX;

#if SVGF_ENABLED
    float3 radianceDirect   = 0.0f;
    float3 radianceIndirect = 0.0f;
    float3 directAlbedo     = 0.0f;
    float3 indirectAlbedo   = 0.0f;
#else
    float3 radiance = 0.0f;
#endif

    float3 throughput = 1.0f;
    float pdf = 1.0f;
    for (int bounce = 0; bounce < PathTracingPassCB.bounceCount; ++bounce)
    {
        HitInfo hit = (HitInfo)0;
        if (TraceRay(ray, hit))
        {
            Instance instanceData = GetInstanceData(hit.instanceIndex);
            Mesh meshData = GetMeshData(instanceData.meshIndex);
            Material materialData = GetMaterialData(instanceData.materialIdx);
            VertexData vert = LoadVertexData(meshData, hit.primitiveIndex, hit.barycentricCoordinates);

            float3 worldPosition = mul(vert.pos, hit.objectToWorldMatrix).xyz;
            float3 worldNormal   = normalize(mul(vert.nor, (float3x3)transpose(hit.worldToObjectMatrix)));
            float3 V             = -ray.Direction;

            MaterialProperties matProps = GetMaterialProperties(materialData, vert.uv, 0);
            BrdfData brdf = GetBrdfData(matProps);

#if SVGF_ENABLED
            if (bounce == 0)
            {
                directAlbedo   = brdf.Diffuse;
                indirectAlbedo = brdf.Diffuse;
            }
#endif

            int lightIndex = 0;
            float lightWeight = 0.0f;
            if (SampleLightRIS(rng, worldPosition, worldNormal, lightIndex, lightWeight))
            {
                LightInfo lightInfo = LoadLightInfo(lightIndex);
                float vis = TraceShadowRay(lightInfo, worldPosition.xyz);
                float3 wi = normalize(-lightInfo.direction.xyz);
                float NdotL = saturate(dot(worldNormal, wi));
                float3 lightContribution = vis * lightInfo.color.rgb * NdotL;

#if SVGF_ENABLED
                float3 diffuseBRDF_White = DiffuseBRDF(float3(1.0, 1.0, 1.0));
                float3 illumination = lightWeight * (diffuseBRDF_White * lightContribution) * throughput / pdf;

                if (bounce == 0) radianceDirect += illumination;
                else             radianceIndirect += illumination;
#else
                float3 diffuseBRDF = DiffuseBRDF(brdf.Diffuse);
                float3 Ftmp;
                float3 specularBRDF = SpecularBRDF(worldNormal, V, wi, brdf.Specular, brdf.Roughness, Ftmp);
                radiance += lightWeight * (diffuseBRDF * lightContribution + specularBRDF * lightContribution) * throughput / pdf;
#endif
            }

#if SVGF_ENABLED
            if (bounce == 0) radianceDirect += matProps.emissive * throughput / pdf;
            else             radianceIndirect += matProps.emissive * throughput / pdf;
#else
            radiance += matProps.emissive * throughput / pdf;
#endif

            if (bounce == PathTracingPassCB.bounceCount - 1) break;

            float3 wi = GetCosHemisphereSample(rng, worldNormal);
            float3 diffBRDF = DiffuseBRDF(brdf.Diffuse);
            float NdotL = saturate(dot(worldNormal, wi));
            throughput *= diffBRDF * NdotL;
            pdf *= (NdotL / PI);

            ray.Origin = OffsetRay(worldPosition, worldNormal);
            ray.Direction = wi;
            ray.TMin = 1e-2f;
            ray.TMax = FLT_MAX;
        }
        else
        {
            TextureCube envMapTexture = ResourceDescriptorHeap[FrameCB.envMapIdx];
            float3 envVal = envMapTexture.SampleLevel(LinearWrapSampler, ray.Direction, 0).rgb;
#if SVGF_ENABLED
            if (bounce == 0)
            {
                radianceDirect += envVal * throughput / pdf;
                directAlbedo = 1.0f; 
            }
            else
            {
                radianceIndirect += envVal * throughput / pdf;
                indirectAlbedo = 1.0f; 
            }
#else
            radiance += envVal * throughput / pdf;
#endif
            break; 
        }
    } 

#if SVGF_ENABLED
    if (any(isnan(radianceDirect)) || any(isinf(radianceDirect)))   radianceDirect = 0.0f;
    if (any(isnan(radianceIndirect)) || any(isinf(radianceIndirect))) radianceIndirect = 0.0f;
    if (any(isnan(directAlbedo)) || any(isinf(directAlbedo)))     directAlbedo = 0.0f;
    if (any(isnan(indirectAlbedo)) || any(isinf(indirectAlbedo)))   indirectAlbedo = 0.0f;

    directRadianceTex[launchIdx]   = float4(radianceDirect, 1.0f);
    indirectRadianceTex[launchIdx] = float4(radianceIndirect, 1.0f);
    directAlbedoTex[launchIdx]     = float4(directAlbedo, 1.0f);
    indirectAlbedoTex[launchIdx]   = float4(indirectAlbedo, 1.0f);

#else
    float3 prevColor = accumulationTexture[launchIdx].rgb;
    float3 accRadiance = radiance;
    if (PathTracingPassCB.accumulatedFrames > 1)
    {
        accRadiance += prevColor;
    }

    if (any(isnan(accRadiance)) || any(isinf(accRadiance)))
    {
        accRadiance = float3(0,0,0); 
    }

    float3 finalOut = accRadiance / (float)PathTracingPassCB.accumulatedFrames;

    accumulationTexture[launchIdx] = float4(accRadiance, 1.0f);
    outputTexture[launchIdx]       = float4(finalOut, 1.0f);
#endif
}