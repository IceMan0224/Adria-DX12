#ifndef _VOLUMETRIC_FOG_RT_
#define _VOLUMETRIC_FOG_RT_

#define FOG_PHASE_G 0.3f

struct FogVolume
{
    float3  center;
    float3  extents;
    float3  color;
    float   densityBase;
    float   densityChange;
};

struct FogResult
{
    bool   scattered;
    float  scatterT;
    float3 scatterAlbedo;
};

float2 IntersectAABB(float3 rayOrigin, float3 rayDirInv, float3 boxMin, float3 boxMax)
{
    float3 t0 = (boxMin - rayOrigin) * rayDirInv;
    float3 t1 = (boxMax - rayOrigin) * rayDirInv;
    float3 tmin = min(t0, t1);
    float3 tmax = max(t0, t1);
    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    return float2(tNear, tFar);
}

float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cosTheta;
    return (1.0f - g2) / (4.0f * PI * pow(denom, 1.5f));
}

float3 SampleHenyeyGreenstein(float3 forward, float g, inout RNG rng)
{
    float xi1 = RNG_GetNext(rng);
    float xi2 = RNG_GetNext(rng);

    float cosTheta;
    if (abs(g) < 1e-3f)
    {
        cosTheta = 1.0f - 2.0f * xi1;
    }
    else
    {
        float s = (1.0f - g * g) / (1.0f + g - 2.0f * g * xi1);
        cosTheta = (1.0f + g * g - s * s) / (2.0f * g);
    }

    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = 2.0f * PI * xi2;

    float3 w = normalize(forward);
    float3 u = abs(w.y) < 0.999f ? normalize(cross(float3(0, 1, 0), w)) : normalize(cross(float3(1, 0, 0), w));
    float3 v = cross(w, u);

    return normalize(sinTheta * cos(phi) * u + sinTheta * sin(phi) * v + cosTheta * w);
}

float EvaluateFogDensity(FogVolume vol, float3 worldPos)
{
    float3 localPos = (worldPos - vol.center) / vol.extents;
    float normalizedHeight = localPos.y * 0.5f + 0.5f;
    return min(1.0f, vol.densityBase + (1.0f - normalizedHeight) * (1.0f - normalizedHeight) * vol.densityChange);
}

// Delta tracking (Woodcock tracking), samples a scatter event along the ray
FogResult SampleFog(float3 rayOrigin, float3 rayDir, float maxT, inout RNG rng)
{
    FogResult result;
    result.scattered = false;
    result.scatterT = maxT;
    result.scatterAlbedo = 0.0f;

    if (FrameCB.fogVolumeCount <= 0 || FrameCB.fogVolumesIdx < 0) return result;

    StructuredBuffer<FogVolume> fogVolumes = ResourceDescriptorHeap[FrameCB.fogVolumesIdx];
    float3 invDir = rcp(rayDir);

    for (int i = 0; i < FrameCB.fogVolumeCount; ++i)
    {
        FogVolume vol = fogVolumes[i];
        float sigmaMaj = vol.densityBase + vol.densityChange;
        if (sigmaMaj <= 0.0f) continue;

        float3 bmin = vol.center - vol.extents;
        float3 bmax = vol.center + vol.extents;
        float2 tt = IntersectAABB(rayOrigin, invDir, bmin, bmax);
        float tEntry = max(tt.x, 0.0f);
        float tExit  = min(tt.y, min(maxT, result.scatterT));
        if (tEntry >= tExit) continue;

        float t = tEntry;
        for (int step = 0; step < 128; ++step)
        {
            t += -log(max(1e-10f, 1.0f - RNG_GetNext(rng))) / sigmaMaj;
            if (t >= tExit) break;

            float3 samplePos = rayOrigin + t * rayDir;
            float density = EvaluateFogDensity(vol, samplePos);

            if (RNG_GetNext(rng) < density / sigmaMaj)
            {
                result.scattered = true;
                result.scatterT = t;
                result.scatterAlbedo = vol.color;
                break;
            }
        }
    }

    return result;
}

// Ratio tracking, unbiased transmittance estimate for shadow rays
float3 GetFogTransmittance(float3 rayOrigin, float3 rayDir, float maxT, inout RNG rng)
{
    if (FrameCB.fogVolumeCount <= 0 || FrameCB.fogVolumesIdx < 0) return 1.0f;

    StructuredBuffer<FogVolume> fogVolumes = ResourceDescriptorHeap[FrameCB.fogVolumesIdx];
    float3 invDir = rcp(rayDir);
    float3 transmittance = 1.0f;

    for (int i = 0; i < FrameCB.fogVolumeCount; ++i)
    {
        FogVolume vol = fogVolumes[i];
        float sigmaMaj = vol.densityBase + vol.densityChange;
        if (sigmaMaj <= 0.0f) continue;

        float3 bmin = vol.center - vol.extents;
        float3 bmax = vol.center + vol.extents;
        float2 tt = IntersectAABB(rayOrigin, invDir, bmin, bmax);
        float tEntry = max(tt.x, 0.0f);
        float tExit  = min(tt.y, maxT);
        if (tEntry >= tExit) continue;

        float t = tEntry;
        for (int step = 0; step < 128; ++step)
        {
            t += -log(max(1e-10f, 1.0f - RNG_GetNext(rng))) / sigmaMaj;
            if (t >= tExit) break;

            float3 samplePos = rayOrigin + t * rayDir;
            float density = EvaluateFogDensity(vol, samplePos);
            transmittance *= 1.0f - density / sigmaMaj;

            if (all(transmittance < 1e-4f)) return 0.0f;
        }
    }

    return transmittance;
}

#endif
