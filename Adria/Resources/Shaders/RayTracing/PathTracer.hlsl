#include "PathTracing.hlsli"

struct PathTracingConstants
{
    int  bounceCount;
    int  accumulatedFrames;
    uint accumIdx;
    uint outputIdx;
    uint albedoIdx;
    uint normalIdx;
    uint motionVectorsIdx;
    uint depthIdx;
    uint meshIDIdx;
    uint specularIdx; 
};
ConstantBuffer<PathTracingConstants> PathTracingPassCB : register(b2);

[shader("raygeneration")]
void PT_RayGen()
{
    RWTexture2D<float4> accumulationTexture = ResourceDescriptorHeap[PathTracingPassCB.accumIdx];

    float2 pixel = float2(DispatchRaysIndex().xy);
    float2 resolution = float2(DispatchRaysDimensions().xy);

    RNG rng = RNG_Initialize(pixel.x + pixel.y * resolution.x, FrameCB.frameCount, 16);
    float2 offset = float2(RNG_GetNext(rng), RNG_GetNext(rng));
    pixel += lerp(-0.5f.xx, 0.5f.xx, offset);

    float2 ncdXY = (pixel / (resolution * 0.5f)) - 1.0f;
    ncdXY.y *= -1.0f;
    float4 rayStart = mul(float4(ncdXY, 1.0f, 1.0f), FrameCB.inverseViewProjection);
    float4 rayEnd = mul(float4(ncdXY, 0.0f, 1.0f), FrameCB.inverseViewProjection);

    rayStart.xyz /= rayStart.w;
    rayEnd.xyz /= rayEnd.w;
    float3 rayDir = normalize(rayEnd.xyz - rayStart.xyz);
    float rayLength = length(rayEnd.xyz - rayStart.xyz);

    RayDesc ray;
    ray.Origin = rayStart.xyz;
    ray.Direction = rayDir;
    ray.TMin = 0.0f;
    ray.TMax = FLT_MAX;

    float3 radianceDiffuse = 0.0f;
    float3 radianceSpecular = 0.0f;
    float3 throughput = 1.0f;

#if WITH_DENOISER
    float4 albedoColor = 0.0f;
    float4 normal = 0.0f;
    float depth = 0.0f;
    float2 motionVector = 0.0f;
    uint meshID = 0;
#endif
    float pdf = 1.0;
    for (int i = 0; i < PathTracingPassCB.bounceCount; ++i)
    {
        HitInfo info = (HitInfo)0;
        if (TraceRay(ray, info))
        {
            Instance instanceData = GetInstanceData(info.instanceIndex);
            Mesh meshData = GetMeshData(instanceData.meshIndex);
            Material materialData = GetMaterialData(instanceData.materialIdx);
            VertexData vertex = LoadVertexData(meshData, info.primitiveIndex, info.barycentricCoordinates);

            float3 worldPosition = mul(vertex.pos, info.objectToWorldMatrix).xyz;
            float3 worldNormal = normalize(mul(vertex.nor, (float3x3) transpose(info.worldToObjectMatrix)));
            float3 geometryNormal = normalize(worldNormal);
            float3 V = -ray.Direction;
            MaterialProperties matProperties = GetMaterialProperties(materialData, vertex.uv, 0);
            BrdfData brdfData = GetBrdfData(matProperties);

#if WITH_DENOISER
            if (i == 0)
            {
                albedoColor = float4(matProperties.baseColor, 1.0f);
                normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);

                float4 clipPos = mul(float4(worldPosition, 1.0), FrameCB.viewProjection);
                depth = clipPos.z / clipPos.w;
                
                float4 previousClipPos = mul(float4(worldPosition, 1.0), FrameCB.prevViewProjection);
                float2 previousUv = (previousClipPos.xy / previousClipPos.w) * float2(0.5, -0.5) + 0.5;
                float2 currentUv = (clipPos.xy / clipPos.w) * float2(0.5, -0.5) + 0.5;
                motionVector = currentUv - previousUv;

                meshID = instanceData.meshIndex;
            }
#endif

            int lightIndex = 0;
            float lightWeight = 0.0f;

            float3 wo = normalize(FrameCB.cameraPosition.xyz - worldPosition);
            if (SampleLightRIS(rng, worldPosition, worldNormal, lightIndex, lightWeight))
            {
                  LightInfo lightInfo = LoadLightInfo(lightIndex); 
                  float visibility = TraceShadowRay(lightInfo, worldPosition.xyz);
                  float3 wi = normalize(-lightInfo.direction.xyz);
                  float NdotL = saturate(dot(worldNormal, wi));
                  float3 lightContribution = visibility * lightInfo.color.rgb * NdotL;

                  float3 diffuseBRDF = DiffuseBRDF(brdfData.Diffuse);
                  float3 F;
                  float3 specularBRDF = SpecularBRDF(worldNormal, wo, wi, brdfData.Specular, brdfData.Roughness, F);

                  radianceDiffuse += lightWeight * (diffuseBRDF * lightContribution) * throughput / pdf;
                  radianceSpecular += lightWeight * (specularBRDF * lightContribution) * throughput / pdf;
            }

            radianceSpecular += matProperties.emissive * throughput / pdf;

            if (i == PathTracingPassCB.bounceCount - 1) break;

            float probDiffuse = ProbabilityToSampleDiffuse(brdfData.Diffuse, brdfData.Specular);
            bool chooseDiffuse = RNG_GetNext(rng) < probDiffuse;

            float3 wi;
            if (chooseDiffuse)
            {
                wi = GetCosHemisphereSample(rng, worldNormal);
                float3 diffuseBrdf = DiffuseBRDF(brdfData.Diffuse);
                float NdotL = saturate(dot(worldNormal, wi));
                throughput *= diffuseBrdf * NdotL;
                pdf *= (NdotL / M_PI) * probDiffuse;
            }
            else
            {
                float2 u = float2(RNG_GetNext(rng), RNG_GetNext(rng));
                float3 H = SampleGGX(u, brdfData.Roughness, worldNormal);
                float roughness = max(brdfData.Roughness, 0.065);
                wi = reflect(-wo, H);

                float3 F;
                float3 specularBrdf = SpecularBRDF(worldNormal, wo, wi, brdfData.Specular, roughness, F);
                float NdotL = saturate(dot(worldNormal, wi));
                throughput *= specularBrdf * NdotL;

                float a = roughness * roughness;
                float D = D_GGX(worldNormal, H, a);
                float NdotH = saturate(dot(worldNormal, H));
                float LdotH = saturate(dot(wi, H));
                float NdotV = saturate(dot(worldNormal, wo));
                float samplePDF = D * NdotH / (4 * LdotH);
                pdf *= samplePDF * (1.0 - probDiffuse);
            }

            ray.Origin = OffsetRay(worldPosition, worldNormal);
            ray.Direction = wi;
            ray.TMin = 1e-2f;
            ray.TMax = FLT_MAX;
        }
        else
        {
            TextureCube envMapTexture = ResourceDescriptorHeap[FrameCB.envMapIdx];
            float3 envMapValue = envMapTexture.SampleLevel(LinearWrapSampler, ray.Direction, 0).rgb;
            
            radianceSpecular += envMapValue * throughput / pdf;

#if WITH_DENOISER
            if (i == 0)
            {
                albedoColor = float4(1.0, 1.0, 1.0, 1.0); 
                meshID = uint(-1);
            }
#endif
            break;
        }
    }
    float3 radiance = radianceDiffuse + radianceSpecular;

    float3 accumulatedRadiance = radiance;
    float3 previousColor = accumulationTexture[DispatchRaysIndex().xy].rgb;
    if (PathTracingPassCB.accumulatedFrames > 1)
    {
        accumulatedRadiance += previousColor;
    }

    if (any(isnan(accumulatedRadiance)) || any(isinf(accumulatedRadiance)))
    {
        accumulatedRadiance = float3(1, 0, 0);
    }

#if WITH_DENOISER
    RWTexture2D<float4> albedoTexture = ResourceDescriptorHeap[PathTracingPassCB.albedoIdx];
    albedoTexture[DispatchRaysIndex().xy] = albedoColor;
    
    RWTexture2D<float4> normalTexture = ResourceDescriptorHeap[PathTracingPassCB.normalIdx];
    normalTexture[DispatchRaysIndex().xy] = normal;
    
    RWTexture2D<float> depthTexture = ResourceDescriptorHeap[PathTracingPassCB.depthIdx];
    depthTexture[DispatchRaysIndex().xy] = depth;
    
    RWTexture2D<float2> motionVectorsTexture = ResourceDescriptorHeap[PathTracingPassCB.motionVectorsIdx];
    motionVectorsTexture[DispatchRaysIndex().xy] = motionVector;
    
    RWTexture2D<uint> meshIDTexture = ResourceDescriptorHeap[PathTracingPassCB.meshIDIdx];
    meshIDTexture[DispatchRaysIndex().xy] = meshID;

    RWTexture2D<float4> specularTexture = ResourceDescriptorHeap[PathTracingPassCB.specularIdx];
    specularTexture[DispatchRaysIndex().xy] = float4(radianceSpecular, 1.0);

    float3 demodulatedDiffuse = radianceDiffuse / (albedoColor.rgb + 0.001f);
    demodulatedDiffuse = clamp(demodulatedDiffuse, 0.0f, 10.0f);
    float3 finalOutputColor = demodulatedDiffuse;

#else 
    float3 finalOutputColor = accumulatedRadiance / PathTracingPassCB.accumulatedFrames;
#endif

    accumulationTexture[DispatchRaysIndex().xy] = float4(accumulatedRadiance, 1.0);
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[PathTracingPassCB.outputIdx];
    outputTexture[DispatchRaysIndex().xy] = float4(finalOutputColor, 1.0f);
}