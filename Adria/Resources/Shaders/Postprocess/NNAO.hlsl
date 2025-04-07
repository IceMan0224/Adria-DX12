#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Constants.hlsli"

//https://theorangeduck.com/page/neural-network-ambient-occlusion

#define BLOCK_SIZE 16
#define FW 31
#define HW ((FW-1)/2)
#define NSAMPLES 16

static const float4 F0a = float4( 2.364370,  2.399485,  0.889055,  4.055205);
static const float4 F0b = float4(-1.296360, -0.926747, -0.441784, -3.308158);
static const float4 F1a = float4( 1.418117,  1.505182,  1.105307,  1.728971);
static const float4 F1b = float4(-0.491502, -0.789398, -0.328302, -1.141073);
static const float4 F2a = float4( 1.181042,  1.292263,  2.136337,  1.616358);
static const float4 F2b = float4(-0.535625, -0.900996, -0.405372, -1.030838);
static const float4 F3a = float4( 1.317336,  2.012828,  1.945621,  5.841383);
static const float4 F3b = float4(-0.530946, -1.091267, -1.413035, -3.908190);

static const float4 Xmean = float4( 0.000052, -0.000003, -0.000076,  0.004600);
static const float4 Xstd  = float4( 0.047157,  0.052956,  0.030938,  0.056321);
static const float Ymean =  0.000000;
static const float Ystd  =  0.116180;

static const float4x4 W1 = float4x4(
 -0.147624,  0.303306,  0.009158, -0.111847, 
 -0.150471,  0.057305, -0.371759, -0.183312, 
  0.154306, -0.240071, -0.259837,  0.044680, 
 -0.006904,  0.036727,  0.302215, -0.190296);

static const float4x4 W2 = float4x4(
  0.212815,  0.316173,  0.135707, -0.097283, 
  0.028991, -0.166099, -0.478362,  0.189983, 
  0.105671,  0.058121, -0.156021,  0.019879, 
 -0.111834, -0.170316, -0.413203, -0.260882);

static const float4 W3 = float4( 0.774455,  0.778138, -0.318566, -0.523377);

static const float4 b0 = float4( 0.428451,  2.619065,  3.756697,  1.636395);
static const float4 b1 = float4( 0.566310,  1.877808,  1.316716,  1.091115);
static const float4 b2 = float4( 0.033848,  0.036487, -1.316707, -1.067260);
static const float b3  = 0.151472;

static const float4 alpha0 = float4( 0.326746, -0.380245,  0.179183,  0.104307);
static const float4 alpha1 = float4( 0.255981,  0.009228,  0.211068,  0.110055);
static const float4 alpha2 = float4(-0.252365,  0.016463, -0.232611,  0.069798);
static const float alpha3  = -0.553760;

static const float4 beta0 = float4( 0.482399,  0.562806,  0.947146,  0.460560);
static const float4 beta1 = float4( 0.670060,  1.090481,  0.461880,  0.322837);
static const float4 beta2 = float4( 0.760696,  1.016398,  1.686991,  1.744554);
static const float beta3  =  0.777760;

float3 rand(float3 seed)
{
  return 2.0 * frac(sin(dot(seed, float3(12.9898, 78.233, 21.317))) * float3(43758.5453, 21383.21227, 20431.20563))-1.0;
}

float4 prelu(float4 x, float4 alpha, float4 beta) 
{
  return beta * max(x, 0.0) + alpha * min(x, 0.0);
}

float prelu(float x, float alpha, float beta) 
{
  return beta * max(x, 0.0) + alpha * min(x, 0.0);
}

float2 spiral(float t, float l, float o)
{
  float angle = l * 2 * M_PI * (t + o);
  float s, c;
  sincos(angle, s, c);
  return t * float2(c, s);
}

struct NNAOConstants
{
    float radius;
    uint depthIdx;
    uint normalIdx;
    uint outputIdx;
    uint F0Idx;
    uint F1Idx;
    uint F2Idx;
    uint F3Idx;
};
ConstantBuffer<NNAOConstants> NNAOPassCB : register(b1);

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void NNAO_CS(CSInput input)
{
    Texture2D<float4> F0 = ResourceDescriptorHeap[NNAOPassCB.F0Idx];
    Texture2D<float4> F1 = ResourceDescriptorHeap[NNAOPassCB.F1Idx];
    Texture2D<float4> F2 = ResourceDescriptorHeap[NNAOPassCB.F2Idx];
    Texture2D<float4> F3 = ResourceDescriptorHeap[NNAOPassCB.F3Idx];

    Texture2D normalRT = ResourceDescriptorHeap[NNAOPassCB.normalIdx];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[NNAOPassCB.depthIdx];
    RWTexture2D<float> outputTexture = ResourceDescriptorHeap[NNAOPassCB.outputIdx];

    uint2 resolution = uint2(FrameCB.renderResolution);
    float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / resolution;
    float3 viewNormal = DecodeNormalOctahedron(normalRT.Sample(LinearBorderSampler, uv).xy * 2.0f - 1.0f);
    
    float depth = depthTexture.Sample(LinearBorderSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);
    float3 seed = rand(viewPosition);
  
    float4 H0 = 0.0f;
    for (uint i = 0; i < NSAMPLES; i++)
    {    
        float scale = (M_PI/4) * (FW*FW) * ((float(i+1)/float(NSAMPLES+1))/(NSAMPLES/2));
        float2 sampleOffsetDir = spiral(float(i+1)/float(NSAMPLES+1), 2.5, 2*M_PI*seed.x);

        float3 neighborProjectedViewPos = viewPosition + float3(sampleOffsetDir * NNAOPassCB.radius, 0.0f);
        float4 neighborClipPos = mul(float4(neighborProjectedViewPos, 1.0f), FrameCB.projection);
        float2 neighborScreenPos = neighborClipPos.xy / neighborClipPos.w; 
        neighborScreenPos.y = -neighborScreenPos.y;
        float2 neighborUv = (neighborScreenPos * 0.5f) + 0.5f;
        neighborUv = saturate(neighborUv);

        float neighborDepth = depthTexture.SampleLevel(LinearBorderSampler, neighborUv, 0);
        float2 encodedNeighborNormal = normalRT.SampleLevel(LinearBorderSampler, neighborUv, 0).rg;
        float3 neighborActualViewPos = GetViewPosition(neighborUv, neighborDepth);
        float3 neighborViewNormal = DecodeNormalOctahedron(encodedNeighborNormal * 2.0f - 1.0f);

        float2 filterUV = (sampleOffsetDir * (NNAOPassCB.radius / length(sampleOffsetDir + 1e-6)) * HW + HW + 0.5) / (HW * 2 + 2);

        float4 F0value = F0.SampleLevel(LinearBorderSampler, filterUV, 0);
        float4 F1value = F1.SampleLevel(LinearBorderSampler, filterUV, 0);
        float4 F2value = F2.SampleLevel(LinearBorderSampler, filterUV, 0);
        float4 F3value = F3.SampleLevel(LinearBorderSampler, filterUV, 0);
    
        float distAtten = saturate(1.0f - length(neighborActualViewPos - viewPosition) / NNAOPassCB.radius);
        float4 X = distAtten * float4(
            neighborViewNormal - viewNormal,
            (neighborActualViewPos.z - viewPosition.z) / NNAOPassCB.radius
        );  

        float4 X_norm = (X - Xmean) / Xstd;
        float4x4 FilterMatrix = float4x4(
            F0value * F0a + F0b,
            F1value * F1a + F1b,
            F2value * F2a + F2b,
            F3value * F3a + F3b 
        );
        H0 += scale * mul(X_norm, FilterMatrix);
    }

    H0 = prelu(H0 + b0, alpha0, beta0);

    float4 H1 = prelu(mul(H0, W1) + b1, alpha1, beta1); 
    float4 H2 = prelu(mul(H1, W2) + b2, alpha2, beta2); 
    float  Y  = prelu(dot(H2, W3) + b3, alpha3, beta3);

    float ambientOcclusion = 1.0 - saturate(Y * Ystd + Ymean);
    outputTexture[input.DispatchThreadId.xy] = ambientOcclusion;
}