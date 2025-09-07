#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Tonemapping.hlsli"

struct ReprojectionPassConstants
{
    bool reset;
    float alpha;
    float momentsAlpha;
    uint directIllumIdx;
    uint indirectIllumIdx;
    uint motionIdx;
    uint compactNormDepthIdx;
    uint historyDirectIdx;
    uint historyIndirectIdx;
    uint historyMomentsIdx;
    uint historyNormalDepthIdx;
    uint historyLengthIdx;
    uint outputDirectIdx;
    uint outputIndirectIdx;
    uint outputMomentsIdx;
    uint outputNormalDepthIdx;
    uint outputHistoryLengthIdx;
};
ConstantBuffer<ReprojectionPassConstants> ReprojectionPassCB : register(b2);

uint2 PackNormalDepth(float3 normal, float depth)
{
    uint2 packed;
    packed.x = f32tof16(normal.x) | (f32tof16(normal.y) << 16);
    packed.y = f32tof16(normal.z) | (f32tof16(depth) << 16);
    return packed;
}

void UnpackNormalDepth(uint2 packed, out float3 normal, out float depth)
{
    normal.x = f16tof32(packed.x & 0xffff);
    normal.y = f16tof32(packed.x >> 16);
    normal.z = f16tof32(packed.y & 0xffff);
    depth = f16tof32(packed.y >> 16);
}

[numthreads(16, 16, 1)]
void SVGF_ReprojectionCS(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4>  directIllum           = ResourceDescriptorHeap[ReprojectionPassCB.directIllumIdx];
    Texture2D<float4>  indirectIllum         = ResourceDescriptorHeap[ReprojectionPassCB.indirectIllumIdx];
    Texture2D<float4>  motionVectors         = ResourceDescriptorHeap[ReprojectionPassCB.motionIdx];
    Texture2D<float4>  compactNormDepth      = ResourceDescriptorHeap[ReprojectionPassCB.compactNormDepthIdx];
    
    Texture2D<float4>  historyDirect         = ResourceDescriptorHeap[ReprojectionPassCB.historyDirectIdx];
    Texture2D<float4>  historyIndirect       = ResourceDescriptorHeap[ReprojectionPassCB.historyIndirectIdx];
    Texture2D<float2>  historyMoments        = ResourceDescriptorHeap[ReprojectionPassCB.historyMomentsIdx];
    Texture2D<uint2>   historyNormalDepth    = ResourceDescriptorHeap[ReprojectionPassCB.historyNormalDepthIdx];
    Texture2D<float>   historyLength         = ResourceDescriptorHeap[ReprojectionPassCB.historyLengthIdx];
    
    RWTexture2D<float4> outputDirect         = ResourceDescriptorHeap[ReprojectionPassCB.outputDirectIdx];
    RWTexture2D<float4> outputIndirect       = ResourceDescriptorHeap[ReprojectionPassCB.outputIndirectIdx];
    RWTexture2D<float2> outputMoments        = ResourceDescriptorHeap[ReprojectionPassCB.outputMomentsIdx];
    RWTexture2D<uint2>  outputNormalDepth    = ResourceDescriptorHeap[ReprojectionPassCB.outputNormalDepthIdx];
    RWTexture2D<float>  outputHistoryLength  = ResourceDescriptorHeap[ReprojectionPassCB.outputHistoryLengthIdx];

    int2 pix = (int2)DTid.xy;
    float2 uv = (pix + 0.5) / FrameCB.displayResolution;

    float4 compactData = compactNormDepth.Load(int3(pix, 0));
    uint packedShadingNormal = asuint(compactData.x);
    float3 normalCurrent = DecodeNormal16x2(packedShadingNormal);
    float depthCurrent = compactData.y;
    float depthFwidth = compactData.z;

    float3 directCurrent = directIllum.Load(int3(pix, 0)).rgb;
    float3 indirectCurrent = indirectIllum.Load(int3(pix, 0)).rgb;

    float4 motionData = motionVectors.Load(int3(pix, 0));
    float2 motion = motionData.xy;

    float2 prevUv = uv - motion;

    float4 prevDirectIllum = 0;
    float4 prevIndirectIllum = 0;
    float2 prevMoments = 0;
    float prevHistoryLen = 0;
    bool validHistory = !ReprojectionPassCB.reset && all(prevUv >= 0) && all(prevUv <= 1);

    if (validHistory)
    {
        uint2 packedPrevNd = historyNormalDepth.SampleLevel(PointClampSampler, prevUv, 0);
        float3 prevNormal;
        float prevDepth;
        UnpackNormalDepth(packedPrevNd, prevNormal, prevDepth);
        
        if (dot(normalCurrent, prevNormal) < 0.8 || abs(depthCurrent - prevDepth) / (depthFwidth + 1e-4f) > 4.0f)
        {
            validHistory = false;
        }
        else
        {
            prevDirectIllum   = historyDirect.SampleLevel(LinearClampSampler, prevUv, 0);
            prevIndirectIllum = historyIndirect.SampleLevel(LinearClampSampler, prevUv, 0);
            prevMoments       = historyMoments.SampleLevel(LinearClampSampler, prevUv, 0);
            prevHistoryLen    = historyLength.SampleLevel(PointClampSampler, prevUv, 0);
        }
    }
    
    float currentHistoryLength = min(32.0f, validHistory ? prevHistoryLen + 1.0f : 1.0f);

    float alpha = validHistory ? max(ReprojectionPassCB.alpha, 1.0f / currentHistoryLength) : 1.0f;
    float momentsAlpha = validHistory ? max(ReprojectionPassCB.momentsAlpha, 1.0f / currentHistoryLength) : 1.0f;

    float lumDirect = Luminance(directCurrent);
    float2 momentsCurrent = float2(lumDirect, lumDirect * lumDirect); 

    float4 lerpedDirect = lerp(prevDirectIllum, float4(directCurrent, 0.0), alpha);
    float4 lerpedIndirect = lerp(prevIndirectIllum, float4(indirectCurrent, 0.0), alpha);
    float2 lerpedMoments = lerp(prevMoments, momentsCurrent, momentsAlpha);
    
    float mu1 = lerpedMoments.x;
    float mu2 = lerpedMoments.y;
    float variance = max(0.0, mu2 - mu1 * mu1);
    
    outputDirect[pix] = float4(lerpedDirect.rgb, variance);
    outputIndirect[pix] = lerpedIndirect; 
    outputMoments[pix] = lerpedMoments;
    outputNormalDepth[pix] = PackNormalDepth(normalCurrent, depthCurrent);
    outputHistoryLength[pix] = currentHistoryLength;
}

struct FilterMomentsConstants
{
    float phiColor;
    float phiNormal;
    uint directIllumIdx;
    uint indirectIllumIdx;
    uint momentsIdx;
    uint historyLengthIdx;
    uint compactNormDepthIdx;
    uint outputDirectIdx;
    uint outputIndirectIdx;
};
ConstantBuffer<FilterMomentsConstants> FilterMomentsCB : register(b2);

float BilateralWeight_Moments(float depthDiff, float phiDepth, float normalDot, float phiNormal, float lumaDiff, float phiLuma) 
{
    float w = exp(-depthDiff / phiDepth);
    w *= pow(max(0.0, normalDot), phiNormal);
    w *= exp(-lumaDiff / phiLuma);
    return w;
}

[numthreads(16, 16, 1)]
void SVGF_FilterMomentsCS(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4>  directIllum           = ResourceDescriptorHeap[FilterMomentsCB.directIllumIdx];
    Texture2D<float4>  indirectIllum         = ResourceDescriptorHeap[FilterMomentsCB.indirectIllumIdx];
    Texture2D<float2>  moments               = ResourceDescriptorHeap[FilterMomentsCB.momentsIdx];
    Texture2D<float>   historyLength         = ResourceDescriptorHeap[FilterMomentsCB.historyLengthIdx];
    Texture2D<float4>  compactNormDepth      = ResourceDescriptorHeap[FilterMomentsCB.compactNormDepthIdx];
    RWTexture2D<float4> outputDirect         = ResourceDescriptorHeap[FilterMomentsCB.outputDirectIdx];
    RWTexture2D<float4> outputIndirect       = ResourceDescriptorHeap[FilterMomentsCB.outputIndirectIdx];

    int2 ipos = (int2)DTid.xy;
    float h = historyLength.Load(int3(ipos, 0));

    float4 directIn = directIllum.Load(int3(ipos, 0));
    float4 indirectIn = indirectIllum.Load(int3(ipos, 0));

    if (h >= 4.0)
    {
        outputDirect[ipos] = directIn;
        outputIndirect[ipos] = indirectIn;
        return;
    }

    float4 centerCompactData = compactNormDepth.Load(int3(ipos, 0));
    uint packedCenterNormal = asuint(centerCompactData.x);
    float3 centerNormal = DecodeNormal16x2(packedCenterNormal);
    float centerDepth = centerCompactData.y;
    float centerDepthFwidth = centerCompactData.z;

    float3 centerDirectColor = directIn.rgb;
    float centerLuma = Luminance(centerDirectColor);

    float sumW = 0.0;
    float3 sumDirect = 0.0;
    float3 sumIndirect = 0.0;
    float2 sumMoments = 0.0;
    
    const int radius = 3;
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            int2 samplePos = ipos + int2(x, y);

            float4 sampleCompactData = compactNormDepth.Load(int3(samplePos, 0));
            uint packedSampleNormal = asuint(sampleCompactData.x);
            float3 sampleNormal = DecodeNormal16x2(packedSampleNormal);
            float sampleDepth = sampleCompactData.y;

            float depthDiff = abs(centerDepth - sampleDepth);
            float normalDot = dot(centerNormal, sampleNormal);

            float3 sampleDirectColor = directIllum.Load(int3(samplePos, 0)).rgb;
            float sampleLuma = Luminance(sampleDirectColor);
            float lumaDiff = abs(centerLuma - sampleLuma);
            
            float phiDepth = centerDepthFwidth * 3.0f;
            float weight = BilateralWeight_Moments(depthDiff, phiDepth + 1e-6, normalDot, FilterMomentsCB.phiNormal, lumaDiff, FilterMomentsCB.phiColor + 1e-6);

            sumW += weight;
            sumDirect += sampleDirectColor * weight;
            sumIndirect += indirectIllum.Load(int3(samplePos, 0)).rgb * weight;
            sumMoments += moments.Load(int3(samplePos, 0)) * weight;
        }
    }

    sumW = max(sumW, 1e-6f);
    sumDirect /= sumW;
    sumIndirect /= sumW;
    sumMoments /= sumW;

    float variance = sumMoments.y - sumMoments.x * sumMoments.x;
    variance = max(0.0, variance);
    variance *= 4.0 / max(h, 1.0); 

    outputDirect[ipos] = float4(sumDirect, variance);
    outputIndirect[ipos] = float4(sumIndirect, 0.0);
}


struct AtrousPassConstants
{
    int   stepSize;
    bool  performModulation;
    float phiColor;
    float phiNormal;
    float phiDepth;
    uint  directInIdx;
    uint  indirectInIdx;
    uint  historyLengthIdx;
    uint  compactNormDepthIdx;
    uint  directAlbedoIdx;
    uint  indirectAlbedoIdx;
    uint  directOutIdx;
    uint  indirectOutIdx;
    uint  feedbackDirectOutIdx;
    uint  feedbackIndirectOutIdx;
};
ConstantBuffer<AtrousPassConstants> AtrousPassCB : register(b2);

float ComputeStabilizedVariance(int2 ipos, Texture2D<float4> directTex)
{
    float sum = 0.0;
    const float kernel[2][2] = { { 1.0 / 4.0, 1.0 / 8.0 }, { 1.0 / 8.0, 1.0 / 16.0 } };
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float k = kernel[abs(x)][abs(y)];
            sum += directTex.Load(int3(ipos + int2(x, y), 0)).a * k;
        }
    }
    return sum;
}

[numthreads(16, 16, 1)]
void SVGF_AtrousCS(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4>  directIn            = ResourceDescriptorHeap[AtrousPassCB.directInIdx];
    Texture2D<float4>  indirectIn          = ResourceDescriptorHeap[AtrousPassCB.indirectInIdx];
    Texture2D<float>   historyLength       = ResourceDescriptorHeap[AtrousPassCB.historyLengthIdx];
    Texture2D<float4>  compactNormDepth    = ResourceDescriptorHeap[AtrousPassCB.compactNormDepthIdx];
    Texture2D<float4>  directAlbedo        = ResourceDescriptorHeap[AtrousPassCB.directAlbedoIdx];
    Texture2D<float4>  indirectAlbedo      = ResourceDescriptorHeap[AtrousPassCB.indirectAlbedoIdx];
    RWTexture2D<float4> directOut          = ResourceDescriptorHeap[AtrousPassCB.directOutIdx];

    int2 ipos = (int2)DTid.xy;
    int step = AtrousPassCB.stepSize;

    float4 centerDirect = directIn.Load(int3(ipos, 0));
    float4 centerIndirect = indirectIn.Load(int3(ipos, 0));

    float4 centerCompactData = compactNormDepth.Load(int3(ipos, 0));
    uint packedCenterNormal = asuint(centerCompactData.x);
    float3 centerNormal = DecodeNormal16x2(packedCenterNormal);
    float centerDepth = centerCompactData.y;
    float centerDepthFwidth = centerCompactData.z;
    
    float variance = ComputeStabilizedVariance(ipos, directIn);
    float adaptivePhiColor = AtrousPassCB.phiColor * sqrt(max(1e-5, variance));
    
    float sumWDirect = 1.0;
    float sumWIndirect = 1.0;
    float4 sumDirect = centerDirect;
    float4 sumIndirect = centerIndirect;

    const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            if (x == 0 && y == 0) continue;

            int2 samplePos = ipos + int2(x, y) * step;
            
            float4 sampleDirect = directIn.Load(int3(samplePos, 0));
            float4 sampleIndirect = indirectIn.Load(int3(samplePos, 0));
            
            float4 sampleCompactData = compactNormDepth.Load(int3(samplePos, 0));
            uint packedSampleNormal = asuint(sampleCompactData.x);
            float3 sampleNormal = DecodeNormal16x2(packedSampleNormal);
            float sampleDepth = sampleCompactData.y;
            
            float depthDiff = abs(centerDepth - sampleDepth);
            float normalDot = dot(centerNormal, sampleNormal);

            float lumaDiffDirect = abs(Luminance(centerDirect.rgb) - Luminance(sampleDirect.rgb));
            float lumaDiffIndirect = abs(Luminance(centerIndirect.rgb) - Luminance(sampleIndirect.rgb));
            
            float phiDepth = centerDepthFwidth * AtrousPassCB.phiDepth * float(step);
            
            float wDirect = BilateralWeight_Moments(depthDiff, phiDepth + 1e-6, normalDot, AtrousPassCB.phiNormal, lumaDiffDirect, adaptivePhiColor + 1e-6);
            float wIndirect = BilateralWeight_Moments(depthDiff, phiDepth + 1e-6, normalDot, AtrousPassCB.phiNormal, lumaDiffIndirect, AtrousPassCB.phiColor + 1e-6);
            
            float kernel = kernelWeights[abs(x)] * kernelWeights[abs(y)];
            wDirect *= kernel;
            wIndirect *= kernel;

            sumWDirect += wDirect;
            sumDirect += float4(sampleDirect.rgb * wDirect, sampleDirect.a * wDirect * wDirect);

            sumWIndirect += wIndirect;
            sumIndirect += float4(sampleIndirect.rgb * wIndirect, sampleIndirect.a * wIndirect * wIndirect);
        }
    }
    
    float invSumWDirectSq = 1.0 / (sumWDirect * sumWDirect);
    float invSumWIndirectSq = 1.0 / (sumWIndirect * sumWIndirect);
    sumDirect.rgb /= sumWDirect;
    sumDirect.a *= invSumWDirectSq;

    sumIndirect.rgb /= sumWIndirect;
    sumIndirect.a *= invSumWIndirectSq;

    if (AtrousPassCB.performModulation)
    {
        RWTexture2D<float4> feedbackDirectOut = ResourceDescriptorHeap[AtrousPassCB.feedbackDirectOutIdx];
        RWTexture2D<float4> feedbackIndirectOut = ResourceDescriptorHeap[AtrousPassCB.feedbackIndirectOutIdx];
        feedbackDirectOut[ipos] = sumDirect;
        feedbackIndirectOut[ipos] = sumIndirect;

        float3 directAlbedoColor = directAlbedo.Load(int3(ipos, 0)).rgb;
        float3 indirectAlbedoColor = indirectAlbedo.Load(int3(ipos, 0)).rgb;
        float3 finalColor = sumDirect.rgb * directAlbedoColor + sumIndirect.rgb * indirectAlbedoColor;
        directOut[ipos] = float4(finalColor, 1.0);
    }
    else
    {
        RWTexture2D<float4> indirectOut = ResourceDescriptorHeap[AtrousPassCB.indirectOutIdx];
        directOut[ipos] = sumDirect;
        indirectOut[ipos] = sumIndirect;
    }
}