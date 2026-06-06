#include "ReSTIR/ReSTIR_Util.hlsli"

//reference: https://github.com/NVIDIA-RTX/RTXDI-Library/blob/main/Include/Rtxdi/GI/Reservoir.hlsli

static const float ReSTIR_GI_FireflyClamp = 10.0f;
static const float ReSTIR_GI_MaxSampleRadiance = 20.0f;
static const float ReSTIR_GI_MaxJacobian = 10.0f;

// A GI reservoir stores a secondary surface sample (sample point) rather than a light reference.
// The sample point is the world position seen by tracing a BRDF ray from the primary surface,
// together with its normal and the radiance it emits back towards the primary surface.
struct ReSTIR_GI_Reservoir
{
    float3 position;   // Secondary surface world position (the sample point)
    float  weightSum;  // Overloaded: RIS weight sum during streaming, then inverse PDF after FinalizeResampling
    float3 normal;     // Secondary surface world normal
    float  targetPdf;  // Target PDF of the selected sample, evaluated at the owning surface
    float3 radiance;   // Outgoing radiance from the sample point towards the primary surface
    float  M;          // Number of samples considered (pairwise MIS makes this a float)
    float  age;        // Number of frames since the sample was created, for temporal rejection
    float  pad0;
    float  pad1;
    float  pad2;
};

ReSTIR_GI_Reservoir ReSTIR_GI_EmptyReservoir()
{
    ReSTIR_GI_Reservoir r = (ReSTIR_GI_Reservoir)0;
    return r;
}

bool ReSTIR_GI_IsValid(const ReSTIR_GI_Reservoir reservoir)
{
    return reservoir.M > 0.0f;
}

float ReSTIR_GI_GetInvPdf(const ReSTIR_GI_Reservoir reservoir)
{
    return reservoir.weightSum;
}

// Evaluates the unshadowed contribution of a sample point at a given surface.
float3 ReSTIR_GI_EvaluateContribution(float3 samplePos, float3 sampleRadiance, Surface surface)
{
    float3 toSample = samplePos - surface.worldPos;
    float distSq = dot(toSample, toSample);
    if (distSq < 1e-8f)
    {
        return 0.0f;
    }
    float3 L = toSample * rsqrt(distSq);
    float NdotL = saturate(dot(surface.worldNormal, L));
    if (NdotL <= 0.0f)
    {
        return 0.0f;
    }
    float3 brdf = DefaultBRDF(L, surface.viewDir, surface.worldNormal,
                              surface.brdfData.Diffuse, surface.brdfData.Specular, surface.brdfData.Roughness);
    return brdf * sampleRadiance * NdotL;
}

float ReSTIR_GI_GetSampleTargetPdfForSurface(float3 samplePos, float3 sampleRadiance, Surface surface)
{
    return CalculateLuminance(ReSTIR_GI_EvaluateContribution(samplePos, sampleRadiance, surface));
}

// Reconnection Jacobian for reusing a sample point that was originally seen from `neighborPos`
// but is now connected to `currentPos`. Accounts for the change in solid angle at the sample point.
float ReSTIR_GI_GetReconnectionJacobian(float3 currentPos, float3 neighborPos, const ReSTIR_GI_Reservoir reservoir)
{
    float3 toCurrent = reservoir.position - currentPos;
    float3 toNeighbor = reservoir.position - neighborPos;
    float currentDistSq = dot(toCurrent, toCurrent);
    float neighborDistSq = dot(toNeighbor, toNeighbor);
    if (currentDistSq < 1e-8f || neighborDistSq < 1e-8f)
    {
        return 0.0f;
    }
    float cosCurrent = saturate(dot(reservoir.normal, -toCurrent * rsqrt(currentDistSq)));
    float cosNeighbor = saturate(dot(reservoir.normal, -toNeighbor * rsqrt(neighborDistSq)));
    if (cosNeighbor <= 0.0f)
    {
        return 0.0f;
    }
    return (cosCurrent * neighborDistSq) / (cosNeighbor * currentDistSq);
}

// Adds a new, non-reservoir sample into the reservoir, returns true if this sample was selected.
// Algorithm (3) from the ReSTIR paper, Streaming RIS using weighted reservoir sampling.
bool ReSTIR_GI_StreamSample(
    inout ReSTIR_GI_Reservoir reservoir,
    float3 samplePos,
    float3 sampleNormal,
    float3 sampleRadiance,
    float random,
    float targetPdf,
    float invSourcePdf)
{
    float risWeight = targetPdf * invSourcePdf;
    reservoir.M += 1;
    reservoir.weightSum += risWeight;

    bool selectSample = (random * reservoir.weightSum < risWeight);
    if (selectSample)
    {
        reservoir.position = samplePos;
        reservoir.normal = sampleNormal;
        reservoir.radiance = sampleRadiance;
        reservoir.targetPdf = targetPdf;
    }
    return selectSample;
}

// Adds `newReservoir` into `reservoir`, returns true if the new reservoir's sample was selected.
// Algorithm (4) from the ReSTIR paper, Combining the streams of multiple reservoirs.
bool ReSTIR_GI_CombineReservoirs(
    inout ReSTIR_GI_Reservoir reservoir,
    const ReSTIR_GI_Reservoir newReservoir,
    float random,
    float targetPdf)
{
    if (newReservoir.M <= 0.0f || targetPdf <= 0.0f)
    {
        return false;
    }

    float risWeight = targetPdf * newReservoir.weightSum * newReservoir.M;
    reservoir.M += newReservoir.M;
    reservoir.weightSum += risWeight;

    bool selectSample = (random * reservoir.weightSum < risWeight);
    if (selectSample)
    {
        reservoir.position = newReservoir.position;
        reservoir.normal = newReservoir.normal;
        reservoir.radiance = newReservoir.radiance;
        reservoir.targetPdf = targetPdf;
    }
    return selectSample;
}

// Performs normalization of the reservoir after streaming. Equation (6) from the ReSTIR paper.
void ReSTIR_GI_FinalizeResampling(
    inout ReSTIR_GI_Reservoir reservoir,
    float normalizationNumerator,
    float normalizationDenominator)
{
    float denominator = reservoir.targetPdf * normalizationDenominator;
    reservoir.weightSum = (denominator == 0.0f) ? 0.0f : (reservoir.weightSum * normalizationNumerator) / denominator;
}

void ReSTIR_GI_StoreReservoir(
    const ReSTIR_GI_Reservoir reservoir,
    uint2 pixelPosition, uint reservoirBufferIdx)
{
    uint flattenIndex = pixelPosition.y * (uint)FrameCB.renderResolution.x + pixelPosition.x;
    RWStructuredBuffer<ReSTIR_GI_Reservoir> reservoirBuffer = ResourceDescriptorHeap[reservoirBufferIdx];
    reservoirBuffer[flattenIndex] = reservoir;
}

ReSTIR_GI_Reservoir ReSTIR_GI_LoadReservoir(uint2 pixelPosition, uint reservoirBufferIdx)
{
    uint flattenIndex = pixelPosition.y * (uint)FrameCB.renderResolution.x + pixelPosition.x;
    StructuredBuffer<ReSTIR_GI_Reservoir> reservoirBuffer = ResourceDescriptorHeap[reservoirBufferIdx];
    return reservoirBuffer[flattenIndex];
}

ReSTIR_GI_Reservoir ReSTIR_GI_LoadReservoirRW(uint2 pixelPosition, uint reservoirBufferIdx)
{
    uint flattenIndex = pixelPosition.y * (uint)FrameCB.renderResolution.x + pixelPosition.x;
    RWStructuredBuffer<ReSTIR_GI_Reservoir> reservoirBuffer = ResourceDescriptorHeap[reservoirBufferIdx];
    return reservoirBuffer[flattenIndex];
}
