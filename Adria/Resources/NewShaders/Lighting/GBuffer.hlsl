#include "../CommonResources.hlsli"


struct VS_INPUT
{
    float3 Position : POSITION;
    float2 Uvs : TEX;
    float3 Normal : NORMAL;
    float3 Tan : TANGENT;
    float3 Bitan : BITANGENT;

};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;

    float2 Uvs          : TEX;
    float3 NormalVS     : NORMAL0;
    float3 TangentWS    : TANGENT;
    float3 BitangentWS  : BITANGENT;
    float3 NormalWS     : NORMAL1;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;

    float2 Uvs : TEX;
    float3 NormalVS : NORMAL0;
    float3 TangentWS : TANGENT;
    float3 BitangentWS : BITANGENT;
    float3 NormalWS : NORMAL1;
    
    bool IsFrontFace : SV_IsFrontFace;
};

struct PS_OUTPUT
{
    float4 NormalMetallic : SV_TARGET0;
    float4 DiffuseRoughness : SV_TARGET1;
    float4 Emissive : SV_TARGET2;
};

struct ModelCBuffer
{
    row_major matrix modelMatrix;
    row_major matrix transposedInverseModelMatrix;
    
    uint albedoIdx;
    uint normalIdx;
    uint metallicRoughnessIdx;
    uint emissiveIdx;
    
    float3 baseColor;
    float  metallicFactor;
    float  roughnessFactor;
    float  emissiveFactor;
    float alphaCutoff;
};
ConstantBuffer<ModelCBuffer> ModelCB : register(b2);

VS_OUTPUT GBufferVS(VS_INPUT input)
{
    VS_OUTPUT Output;
    
    float4 pos = mul(float4(input.Position, 1.0), ModelCB.modelMatrix);
    Output.Position = mul(pos, FrameCB.viewProjection);
    Output.Position.xy += FrameCB.cameraJitter * Output.Position.w;

    Output.Uvs = input.Uvs;

    float3 normal_ws = mul(input.Normal, (float3x3) ModelCB.transposedInverseModelMatrix);
    Output.NormalVS = mul(normal_ws, (float3x3) transpose(FrameCB.inverseView));

    Output.TangentWS = mul(input.Tan, (float3x3) ModelCB.modelMatrix);
    Output.BitangentWS = mul(input.Bitan, (float3x3) ModelCB.modelMatrix);
    Output.NormalWS = normal_ws;
    return Output;
}

PS_OUTPUT PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic)
{
    PS_OUTPUT Out;

    Out.NormalMetallic = float4(0.5 * NormalVS + 0.5, metallic);
    Out.DiffuseRoughness = float4(BaseColor, roughness);
    Out.Emissive = float4(emissive.rgb, emissive.a / 256);
    return Out;
}

PS_OUTPUT GBufferPS(PS_INPUT In)
{
    In.Uvs.y = 1 - In.Uvs.y;
    
    Texture2D txAlbedo              = ResourceDescriptorHeap[ModelCB.albedoIdx];
    Texture2D txNormal              = ResourceDescriptorHeap[ModelCB.normalIdx];
    Texture2D txMetallicRoughness   = ResourceDescriptorHeap[ModelCB.metallicRoughnessIdx];
    Texture2D txEmissive            = ResourceDescriptorHeap[ModelCB.emissiveIdx];
    
    float4 albedoColor = txAlbedo.Sample(LinearWrapSampler, In.Uvs) * float4(ModelCB.baseColor, 1.0f);
#if MASK 
    if(albedoColor.a < 0.5f) discard;
#endif
    
    float3 normal = normalize(In.NormalWS);
    if (!In.IsFrontFace) normal = -normal;
    
    float3 tangent = normalize(In.TangentWS);
    float3 bitangent = normalize(In.BitangentWS);
    float3 bumpMapNormal = txNormal.Sample(LinearWrapSampler, In.Uvs).xyz;
    bumpMapNormal = 2.0f * bumpMapNormal - 1.0f;
    float3x3 TBN = float3x3(tangent, bitangent, normal);
    float3 NewNormal = mul(bumpMapNormal, TBN);
    In.NormalVS = normalize(mul(NewNormal, (float3x3) FrameCB.view));

    float3 aoRoughnessMetallic = txMetallicRoughness.Sample(LinearWrapSampler, In.Uvs).rgb;

    float3 emissiveColor = txEmissive.Sample(LinearWrapSampler, In.Uvs).rgb;
    return PackGBuffer(albedoColor.xyz, normalize(In.NormalVS), float4(emissiveColor, ModelCB.emissiveFactor),
    aoRoughnessMetallic.g * ModelCB.roughnessFactor, aoRoughnessMetallic.b * ModelCB.metallicFactor);
}