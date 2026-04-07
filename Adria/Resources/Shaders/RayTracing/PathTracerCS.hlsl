#include "PathTracing.hlsli"

struct PathTracingConstants
{
    int  bounceCount;
    int  accumulatedFrames;
    uint accumIdx;
    uint outputIdx;
};
ConstantBuffer<PathTracingConstants> PathTracingPassCB : register(b1);

[numthreads(16, 16, 1)]
void PathTracerCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 launchIdx = dispatchThreadId.xy;
    if (launchIdx.x >= (uint)FrameCB.renderResolution.x || launchIdx.y >= (uint)FrameCB.renderResolution.y)
        return;

    float2 resolution = FrameCB.renderResolution;

    RWTexture2D<float4> accumulationTexture = ResourceDescriptorHeap[PathTracingPassCB.accumIdx];
    RWTexture2D<float4> outputTexture       = ResourceDescriptorHeap[PathTracingPassCB.outputIdx];

    uint seedBase = launchIdx.x + launchIdx.y * (uint)resolution.x;
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
    ray.Origin    = rayStart.xyz;
    ray.Direction = rayDir;
    ray.TMin      = 0.0f;
    ray.TMax      = FLT_MAX;

    float3 radiance = 0.0f;

    float3 throughput = 1.0f;
    for (int bounce = 0; bounce < PathTracingPassCB.bounceCount; ++bounce)
    {
        HitInfo hit = (HitInfo)0;
        if (TraceRay(ray, hit))
        {
            Instance instanceData = GetInstanceData(hit.instanceIndex);
            Mesh meshData         = GetMeshData(instanceData.meshIndex);
            Material materialData = GetMaterialData(instanceData.materialIdx);
            VertexDataEx vert = LoadVertexDataEx(meshData, hit.primitiveIndex, hit.barycentricCoordinates);

            float3 worldPosition = mul(vert.pos, hit.objectToWorldMatrix).xyz;
            float3 worldNormal   = normalize(mul(vert.nor, (float3x3)transpose(hit.worldToObjectMatrix)));
            float3 V             = -ray.Direction;

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

            BrdfData brdf = GetBrdfData(matProps);

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

                float3 brdfValue = DefaultBRDF(wi, V, worldNormal, brdf.Diffuse, brdf.Specular, brdf.Roughness);
                radiance += lightWeight * brdfValue * lightRadiance * NdotL * vis * throughput;
            }

            radiance += matProps.emissive * throughput;
            if (bounce == PathTracingPassCB.bounceCount - 1) break;

            if (bounce >= MIN_BOUNCES)
            {
                float survivalProb = min(max(throughput.r, max(throughput.g, throughput.b)), 0.95f);
                if (RNG_GetNext(rng) > survivalProb)
                    break;
                throughput /= survivalProb;
            }

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

            if (combinedPdf < 1e-6f) break;

            float3 bounceBrdf = DefaultBRDF(bounceDir, V, worldNormal, brdf.Diffuse, brdf.Specular, brdf.Roughness);
            throughput *= bounceBrdf * NdotL / combinedPdf;
            throughput = min(throughput, 10.0f);

            ray.Origin    = OffsetRay(worldPosition, worldNormal);
            ray.Direction = bounceDir;
            ray.TMin      = 1e-2f;
            ray.TMax      = FLT_MAX;
        }
        else
        {
            TextureCube envMapTexture = ResourceDescriptorHeap[FrameCB.envMapIdx];
            float3 envVal = envMapTexture.SampleLevel(LinearWrapSampler, ray.Direction, 0).rgb;
            radiance += envVal * throughput;
            break;
        }
    }
    float3 prevColor = accumulationTexture[launchIdx].rgb;
    float3 accRadiance = radiance;
    if (PathTracingPassCB.accumulatedFrames > 1)
    {
        accRadiance += prevColor;
    }

    if (any(isnan(accRadiance)) || any(isinf(accRadiance)))
    {
        accRadiance = float3(0, 0, 0);
    }

    float3 finalOut = accRadiance / (float)PathTracingPassCB.accumulatedFrames;

    accumulationTexture[launchIdx] = float4(accRadiance, 1.0f);
    outputTexture[launchIdx]       = float4(finalOut, 1.0f);
}
