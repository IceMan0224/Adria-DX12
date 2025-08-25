#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Tonemapping.hlsli"

struct ReprojectionPassConstants
{
    bool reset; 
    float alpha; 
    float momentsAlpha; 
    float phiAlbedoRejection;
    uint inputIdx; 
    uint motionIdx; 
    uint depthIdx; 
    uint normalIdx; 
    uint albedoIdx; 
    uint meshIDIdx;
    uint historyColorIdx; 
    uint historyMomentsIdx; 
    uint historyNormalDepthIdx; 
    uint historyMeshIDIdx;
    uint outputColorIdx; 
    uint outputMomentsIdx; 
    uint outputNormalDepthIdx; 
    uint outputMeshIDIdx;
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
    Texture2D<float4>  noisyInput            = ResourceDescriptorHeap[ReprojectionPassCB.inputIdx];
    Texture2D<float2>  motionVectors         = ResourceDescriptorHeap[ReprojectionPassCB.motionIdx];
    Texture2D<float>   depthBuffer           = ResourceDescriptorHeap[ReprojectionPassCB.depthIdx];
    Texture2D<float4>  normalBuffer          = ResourceDescriptorHeap[ReprojectionPassCB.normalIdx];
    Texture2D<float4>  albedoBuffer          = ResourceDescriptorHeap[ReprojectionPassCB.albedoIdx]; 
    Texture2D<uint>    meshIDBuffer          = ResourceDescriptorHeap[ReprojectionPassCB.meshIDIdx];
    Texture2D<float4>  historyColor          = ResourceDescriptorHeap[ReprojectionPassCB.historyColorIdx];
    Texture2D<float2>  historyMoments        = ResourceDescriptorHeap[ReprojectionPassCB.historyMomentsIdx];
    Texture2D<uint2>   historyNormalDepth    = ResourceDescriptorHeap[ReprojectionPassCB.historyNormalDepthIdx];
    Texture2D<uint>    historyMeshID         = ResourceDescriptorHeap[ReprojectionPassCB.historyMeshIDIdx];
    RWTexture2D<float4> outputColor          = ResourceDescriptorHeap[ReprojectionPassCB.outputColorIdx];
    RWTexture2D<float2> outputMoments        = ResourceDescriptorHeap[ReprojectionPassCB.outputMomentsIdx];
    RWTexture2D<uint2>  outputNormalDepth    = ResourceDescriptorHeap[ReprojectionPassCB.outputNormalDepthIdx];
    RWTexture2D<uint>   outputMeshID         = ResourceDescriptorHeap[ReprojectionPassCB.outputMeshIDIdx];

    uint2 pix = DTid.xy;
    float2 uv = (pix + 0.5) / FrameCB.displayResolution;
    
    float depth = depthBuffer.Load(int3(pix, 0));
    float3 normal = DecodeNormalOctahedron(normalBuffer.Load(int3(pix, 0)).xy * 2.0f - 1.0f);
    float3 albedoCurrent = albedoBuffer.Load(int3(pix, 0)).rgb;
    uint meshIDCurrent = meshIDBuffer.Load(int3(pix, 0));
    float3 colorCurrent = noisyInput.Load(int3(pix, 0)).rgb;

    float2 motion = motionVectors.Load(int3(pix, 0));
    float2 prevUv = uv - motion;

    bool validHistory = !ReprojectionPassCB.reset && all(prevUv >= 0) && all(prevUv <= 1);
    float3 colorHistory = 0;
    float2 momentsHistory = 0;

    if (validHistory)
    {
        uint2 packedPrevNd = historyNormalDepth.SampleLevel(PointClampSampler, prevUv, 0);
        float3 prevNormal;
        float prevDepth;
        UnpackNormalDepth(packedPrevNd, prevNormal, prevDepth);
        uint prevMeshID = historyMeshID.SampleLevel(PointClampSampler, prevUv, 0);
        float3 albedoHistory = albedoBuffer.SampleLevel(LinearClampSampler, prevUv, 0).rgb;

        float depthDiff = abs(depth - prevDepth) / max(depth, 0.001);
        float normalDiff = acos(saturate(dot(normal, prevNormal)));
        float albedoDiff = length(albedoCurrent - albedoHistory);

        if (meshIDCurrent != prevMeshID || depthDiff > 0.1 || normalDiff > 0.5 || albedoDiff > ReprojectionPassCB.phiAlbedoRejection)
        {
            validHistory = false;
        }
        else
        {
            colorHistory = historyColor.SampleLevel(LinearClampSampler, prevUv, 0).rgb;
            momentsHistory = historyMoments.SampleLevel(LinearClampSampler, prevUv, 0).xy;
        }
    }

    float lum = Luminance(colorCurrent);
    float2 momentsCurrent = float2(lum, lum * lum); 
    
    float alpha = validHistory ? ReprojectionPassCB.alpha : 1.0;
    float momentsAlpha = validHistory ? ReprojectionPassCB.momentsAlpha : 1.0;

    outputColor[pix] = float4(lerp(colorHistory, colorCurrent, alpha), 1.0);
    outputMoments[pix] = lerp(momentsHistory, momentsCurrent, momentsAlpha);
    outputNormalDepth[pix] = PackNormalDepth(normal, depth);
    outputMeshID[pix] = meshIDCurrent;
}

struct VariancePassConstants 
{ 
    uint colorIdx; 
    uint momentsIdx; 
    uint outputColorIdx; 
    uint outputMomentsIdx; 
};
ConstantBuffer<VariancePassConstants> VariancePassCB : register(b1);

[numthreads(16, 16, 1)]
void SVGF_VarianceCS(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4>   inputColor    = ResourceDescriptorHeap[VariancePassCB.colorIdx];
    Texture2D<float2>   inputMoments  = ResourceDescriptorHeap[VariancePassCB.momentsIdx];
    RWTexture2D<float4> outputColor   = ResourceDescriptorHeap[VariancePassCB.outputColorIdx];
    RWTexture2D<float2> outputMoments = ResourceDescriptorHeap[VariancePassCB.outputMomentsIdx];

    int2 pix = (int2)DTid.xy;
    
    float2 moments = inputMoments.Load(int3(pix, 0));
    float mu1 = moments.x;  
    float mu2 = moments.y;  
    float variance = max(0.0, mu2 - mu1 * mu1);
    
    outputColor[pix] = float4(inputColor.Load(int3(pix, 0)).rgb, variance);
    
    outputMoments[pix] = moments; 
}


struct AtrousPassConstants 
{
    int   stepSize; 
    float phiColor; 
    float phiNormal; 
    float phiDepth; 
    float phiAlbedo;
    uint  inputIdx; 
    uint  momentsIdx; 
    uint  normalIdx; 
    uint  depthIdx; 
    uint  albedoIdx; 
    uint  meshIDIdx; 
    uint  outputIdx; 
    uint  historyColorUpdateIdx;
};
ConstantBuffer<AtrousPassConstants> AtrousPassCB : register(b2);

float BilateralWeight(float diff, float phi) { return exp(-max(0, diff) / phi); }

[numthreads(16, 16, 1)]
void SVGF_AtrousCS(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4>  inputTexture  = ResourceDescriptorHeap[AtrousPassCB.inputIdx];
    Texture2D<float2>  momentsTexture = ResourceDescriptorHeap[AtrousPassCB.momentsIdx];
    Texture2D<float4>  normalTexture = ResourceDescriptorHeap[AtrousPassCB.normalIdx];
    Texture2D<float>   depthTexture  = ResourceDescriptorHeap[AtrousPassCB.depthIdx];
    Texture2D<float4>  albedoTexture = ResourceDescriptorHeap[AtrousPassCB.albedoIdx];
    Texture2D<uint>    meshIDTexture = ResourceDescriptorHeap[AtrousPassCB.meshIDIdx];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[AtrousPassCB.outputIdx];
    
    int2 pix = (int2)DTid.xy;
    int step = AtrousPassCB.stepSize;
    
    float4 centerSample = inputTexture.Load(int3(pix, 0));
    float  centerDepth   = depthTexture.Load(int3(pix, 0));
    float3 centerNormal  = DecodeNormalOctahedron(normalTexture.Load(int3(pix, 0)).xy * 2.0f - 1.0f);
    float3 centerAlbedo  = albedoTexture.Load(int3(pix, 0)).rgb; 
    uint   centerMeshID  = meshIDTexture.Load(int3(pix, 0));
    float  centerLuma    = Luminance(centerSample.rgb);
    
    float stabilizedCenterVariance = 0.0;
    float varianceWeightSum = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 sampleCoord = pix + int2(x, y);
            if (all(sampleCoord >= 0) && all(sampleCoord < FrameCB.displayResolution))
            {
                stabilizedCenterVariance += inputTexture.Load(int3(sampleCoord, 0)).a;
                varianceWeightSum += 1.0;
            }
        }
    }
    if (varianceWeightSum > 0) stabilizedCenterVariance /= varianceWeightSum;
    
    stabilizedCenterVariance = max(stabilizedCenterVariance, 0.001);
    
    float centerKernelWeight = 6.0 / 22.0;
    float neighborKernelWeight = 4.0 / 22.0;
    
    float4 sum = centerSample * centerKernelWeight;
    float weightSum = centerKernelWeight;
    
    int2 kernelOffsets[4] = { int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1) };
    
    [unroll]
    for(int i = 0; i < 4; ++i)
    {
        int2 samplePix = pix + kernelOffsets[i] * step;
        
        if (any(samplePix < 0) || any(samplePix >= FrameCB.displayResolution)) continue;
        if (centerMeshID != meshIDTexture.Load(int3(samplePix, 0))) continue;
        
        float4 sampleValue  = inputTexture.Load(int3(samplePix, 0));
        float  sampleDepth  = depthTexture.Load(int3(samplePix, 0));
        float3 sampleNormal = DecodeNormalOctahedron(normalTexture.Load(int3(samplePix, 0)).xy * 2.0f - 1.0f);
        float3 sampleAlbedo = albedoTexture.Load(int3(samplePix, 0)).rgb;
        float  sampleLuma   = Luminance(sampleValue.rgb);
        
        float lumaDiff   = abs(centerLuma - sampleLuma);
        float depthDiff  = abs(centerDepth - sampleDepth) / max(centerDepth, 0.001);
        float albedoDiff = length(centerAlbedo - sampleAlbedo);
        
        float sampleVariance = max(sampleValue.a, 0.001);
        float minVariance = min(stabilizedCenterVariance, sampleVariance);
        
        float wDepth = BilateralWeight(depthDiff, AtrousPassCB.phiDepth);
        float wNormal = pow(saturate(dot(centerNormal, sampleNormal)), AtrousPassCB.phiNormal);
        
        float adaptiveColorPhi = AtrousPassCB.phiColor * sqrt(max(minVariance, 0.01));
        float wLuma = BilateralWeight(lumaDiff, adaptiveColorPhi);
        float wAlbedo = BilateralWeight(albedoDiff, AtrousPassCB.phiAlbedo);

        float weight = wLuma * wDepth * wNormal * wAlbedo;
        
        weight = max(weight, 0.01);
        
        sum += sampleValue * weight * neighborKernelWeight;
        weightSum += weight * neighborKernelWeight;
    }
    
    float4 finalFiltered = sum / max(weightSum, 1e-6);
    
    float preservationFactor = exp(-stabilizedCenterVariance * 2.0);
    finalFiltered.rgb = lerp(finalFiltered.rgb, centerSample.rgb, preservationFactor * 0.1);
    
    outputTexture[pix] = finalFiltered;

    if (AtrousPassCB.historyColorUpdateIdx != 0)
    {
        RWTexture2D<float4> historyColorUpdateTexture = ResourceDescriptorHeap[AtrousPassCB.historyColorUpdateIdx];
        historyColorUpdateTexture[pix] = float4(finalFiltered.rgb, 1.0);
    }
}