#include "Random.hlsli"
#include "Tonemapping.hlsli"
#include "Surface.hlsli"
#include "LightInfo.hlsli"

static const uint ReSTIR_InvalidLightIndex = uint(-1);

float ReSTIR_DoAttenuation(float distance, float range)
{
    float att = saturate(1.0f - (distance * distance / (range * range)));
    return att * att;
}

float2 ReSTIR_RandomlySelectInfiniteLightUV(inout RNG rng)
{
    float2 uv;
    uv.x = RNG_GetNext(rng);
    uv.y = RNG_GetNext(rng);
    return uv;
}

float2 ReSTIR_RandomlySelectLocalLightUV(inout RNG rng)
{
    float2 uv;
    uv.x = RNG_GetNext(rng);
    uv.y = RNG_GetNext(rng);
    return uv;
}

// Evaluate the surface BRDF using the engine's attenuation model (matching DoLight)
float3 ReSTIR_ShadeSurfaceWithLightSample(LightSample lightSample, LightInfo lightInfo, Surface surface)
{
    float3 toLight = lightSample.position - surface.worldPos;
    float3 L = normalize(toLight);

    float NdotL = saturate(dot(L, surface.worldNormal));
    if (NdotL <= 0)
        return 0;

    float distance = length(toLight);
    float attenuation = 1.0f;
    if (lightInfo.type == POINT_LIGHT || lightInfo.type == SPOT_LIGHT)
    {
        attenuation = ReSTIR_DoAttenuation(distance, lightInfo.range);
    }
    if (attenuation <= 0)
        return 0;

    float3 V = surface.viewDir;
    float diffuse = Lambert(surface.worldNormal, L);
    float3 F;
    float3 specular = SpecularBRDF(surface.worldNormal, V, L, surface.brdfData.Specular, surface.brdfData.Roughness, F);
    float3 brdf = diffuse * surface.brdfData.Diffuse + specular;
    return brdf * NdotL * attenuation * lightInfo.color.rgb;
}

// Compute the target PDF (p-hat) for the given light sample relative to a surface
float ReSTIR_GetLightSampleTargetPdfForSurface(LightSample lightSample, LightInfo lightInfo, Surface surface)
{
    return CalculateLuminance(ReSTIR_ShadeSurfaceWithLightSample(lightSample, lightInfo, surface));
}


LightSample ReSTIR_SampleLight(LightInfo lightInfo, Surface surface, float2 uv)
{
    // light data is stored in view space; transform to world space to match Surface
    lightInfo.position = mul(lightInfo.position, FrameCB.inverseView);
    lightInfo.direction = mul(lightInfo.direction, FrameCB.inverseView);
    LightSample lightSample = CalculateLightSample(lightInfo, uv, surface.worldPos);
    return lightSample;
}