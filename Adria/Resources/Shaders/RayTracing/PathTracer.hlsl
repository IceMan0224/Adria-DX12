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
    for (int bounce = 0; bounce < PathTracingPassCB.bounceCount; ++bounce)
    {
        HitInfo hit = (HitInfo)0;
        if (TraceRay(ray, hit))
        {
            Instance instanceData = GetInstanceData(hit.instanceIndex);
            Mesh meshData = GetMeshData(instanceData.meshIndex);
            Material materialData = GetMaterialData(instanceData.materialIdx);
            VertexDataEx vert = LoadVertexDataEx(meshData, hit.primitiveIndex, hit.barycentricCoordinates);

            float3 worldPosition = mul(vert.pos, hit.objectToWorldMatrix).xyz;
            float3 worldNormal   = normalize(mul(vert.nor, (float3x3)transpose(hit.worldToObjectMatrix)));
            float3 V             = -ray.Direction;

            MaterialProperties matProps = GetMaterialProperties(materialData, vert.uv, 0);

            // Normal mapping - only apply when material has a normal map and tangent data is valid
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

                float3 wi;
                float attenuation = 1.0f;
                if (lightInfo.type == DIRECTIONAL_LIGHT)
                {
                    wi = normalize(-lightInfo.direction.xyz);
                }
                else
                {
                    float3 toLight = lightInfo.position.xyz - worldPosition;
                    float dist = length(toLight);
                    wi = toLight / dist;
                    attenuation = DoAttenuation(dist, lightInfo.range);
                    if (lightInfo.type == SPOT_LIGHT)
                    {
                        float cosAng = dot(-normalize(lightInfo.direction.xyz), wi);
                        float conAtt = saturate((cosAng - lightInfo.outerCosine) / (lightInfo.innerCosine - lightInfo.outerCosine));
                        attenuation *= conAtt * conAtt;
                    }
                }

                float vis = TraceShadowRay(lightInfo, worldPosition.xyz);
                float NdotL = saturate(dot(worldNormal, wi));
                float3 lightRadiance = lightInfo.color.rgb * attenuation;

#if SVGF_ENABLED
                float3 F;
                float3 specBRDF = SpecularBRDF(worldNormal, V, wi, brdf.Specular, brdf.Roughness, F);
                float3 diffBRDF_white = DiffuseBRDF(float3(1.0, 1.0, 1.0)) * (1.0 - F);
                float3 illumination = lightWeight * (diffBRDF_white + specBRDF) * lightRadiance * NdotL * vis * throughput;
                if (bounce == 0) radianceDirect += illumination;
                else             radianceIndirect += illumination;
#else
                float3 brdfValue = DefaultBRDF(wi, V, worldNormal, brdf.Diffuse, brdf.Specular, brdf.Roughness);
                radiance += lightWeight * brdfValue * lightRadiance * NdotL * vis * throughput;
#endif
            }

#if SVGF_ENABLED
            if (bounce == 0) radianceDirect += matProps.emissive * throughput;
            else             radianceIndirect += matProps.emissive * throughput;
#else
            radiance += matProps.emissive * throughput;
#endif

            if (bounce == PathTracingPassCB.bounceCount - 1) break;

            float pDiffuse = ProbabilityToSampleDiffuse(brdf.Diffuse, brdf.Specular);
            float3 bounceDir;
            if (RNG_GetNext(rng) < pDiffuse)
            {
                bounceDir = GetCosHemisphereSample(rng, worldNormal);
            }
            else
            {
                float2 Xi = float2(RNG_GetNext(rng), RNG_GetNext(rng));
                float4 hPdf = ImportanceSampleGGX(Xi, max(brdf.Roughness, MIN_ROUGHNESS));
                float3 H = TangentToWorld(hPdf.xyz, worldNormal);
                bounceDir = reflect(-V, H);
            }

            float NdotL = saturate(dot(worldNormal, bounceDir));
            if (NdotL <= 0.0f) break;

            float diffPdf = NdotL * (1.0f / PI);
            float3 H = normalize(V + bounceDir);
            float NdotH = saturate(dot(worldNormal, H));
            float VdotH = saturate(dot(V, H));
            float a = max(brdf.Roughness, MIN_ROUGHNESS) * max(brdf.Roughness, MIN_ROUGHNESS);
            float D = D_GGX(worldNormal, H, a);
            float specPdf = D * NdotH / max(4.0f * VdotH, 0.001f);
            float combinedPdf = pDiffuse * diffPdf + (1.0f - pDiffuse) * specPdf;

            if (combinedPdf < 1e-6f) 
            {
                break;
            }

            float3 bounceBrdf = DefaultBRDF(bounceDir, V, worldNormal, brdf.Diffuse, brdf.Specular, brdf.Roughness);
            throughput *= bounceBrdf * NdotL / combinedPdf;

            ray.Origin = OffsetRay(worldPosition, worldNormal);
            ray.Direction = bounceDir;
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
                radianceDirect += envVal * throughput;
                directAlbedo = 1.0f;
            }
            else
            {
                radianceIndirect += envVal * throughput;
                indirectAlbedo = 1.0f;
            }
#else
            radiance += envVal * throughput;
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