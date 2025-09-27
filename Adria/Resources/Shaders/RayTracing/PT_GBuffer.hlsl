#include "Scene.hlsli"
#include "Packing.hlsli"


struct PT_GBufferConstants
{
    uint instanceId;
};
ConstantBuffer<PT_GBufferConstants> PT_GBufferPassCB : register(b1);

struct VSToPS
{
	float4 Position     : SV_POSITION;
	float3 PositionWS	: POSITION;
	float2 Uvs          : TEX;
	float3 TangentWS    : TANGENT;
	float3 BitangentWS  : BITANGENT;
	float3 NormalWS     : NORMAL1;
};

struct PSOutput
{
	float4 LinearZ    : SV_Target0;   
	float4 MotionVectors   : SV_Target1;   
	float4 SvgfCompact : SV_Target2;   
};

VSToPS PT_GBufferVS(uint vertexId : SV_VertexID)
{
	VSToPS output = (VSToPS)0;

    Instance instanceData = GetInstanceData(PT_GBufferPassCB.instanceId);
    Mesh meshData = GetMeshData(instanceData.meshIndex);

	float3 pos = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, vertexId);
	float2 uv  = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, vertexId);
	float3 nor = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, vertexId);
	float4 tan = LoadMeshBuffer<float4>(meshData.bufferIdx, meshData.tangentsOffset, vertexId);
    
	float4 posWS = mul(float4(pos, 1.0), instanceData.worldMatrix);
	output.PositionWS = posWS.xyz;
	output.Position = mul(posWS, FrameCB.viewProjection);
	output.Position.xy += FrameCB.cameraJitter * output.Position.w;
	output.Uvs = uv;

	output.NormalWS =  mul(nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
	output.TangentWS = mul(tan.xyz, (float3x3) instanceData.worldMatrix);
	output.BitangentWS = normalize(cross(output.NormalWS, output.TangentWS) * tan.w);
	
	return output;
}

PSOutput PT_GBufferPS(VSToPS input)
{
    Instance instanceData = GetInstanceData(PT_GBufferPassCB.instanceId);
    Material materialData = GetMaterialData(instanceData.materialIdx);

    Texture2D albedoTexture = ResourceDescriptorHeap[materialData.diffuseIdx];
    Texture2D normalTexture = ResourceDescriptorHeap[materialData.normalIdx];
    Texture2D metallicRoughnessTexture = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];
    Texture2D emissiveTexture = ResourceDescriptorHeap[materialData.emissiveIdx];

    PSOutput output = (PSOutput)0;

    float4 albedoColor = albedoTexture.Sample(LinearWrapSampler, input.Uvs) * float4(materialData.baseColorFactor, 1.0f);
    if (albedoColor.a < materialData.alphaCutoff) discard;

    float3 normal = normalize(input.NormalWS);
    float3 tangent = normalize(input.TangentWS);
    float3 bitangent = normalize(input.BitangentWS);

    float3 normalTS = normalTexture.Sample(LinearWrapSampler, input.Uvs).xyz * 2.0f - 1.0f;
    float3x3 TBN = float3x3(tangent, bitangent, normal);
    normal = normalize(mul(normalTS, TBN)); 
    float3 normalOS = normalize(mul(normal, (float3x3)instanceData.inverseWorldMatrix));

    float linearZ = input.Position.z * input.Position.w;
    float maxChangeZ = max(abs(ddx(linearZ)), abs(ddy(linearZ)));

    float4 prevClipPos = mul(float4(input.PositionWS, 1.0f), FrameCB.prevViewProjection);
    prevClipPos.xy += FrameCB.prevCameraJitter * prevClipPos.w; 
    prevClipPos.xyz /= prevClipPos.w; 

    uint packedNormalOS = EncodeNormal16x2(normalOS);
    float packedNormalOS_asFloat = asfloat(packedNormalOS);

    float2 currentPixelPos = input.Position.xy; 
    float2 invFrameSize = rcp(FrameCB.renderResolution);

    float2 prevPosNDC = prevClipPos.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
    float2 currentUV = currentPixelPos * invFrameSize; 

    float2 motionVec = currentUV - prevPosNDC; 

    const float eps = 1e-5;
    if (prevClipPos.w < eps) motionVec = float2(0.0, 0.0); 

    float posFwidth    = length(fwidth(input.PositionWS));
    float normalFwidth = length(fwidth(normal));

    output.MotionVectors = float4(motionVec, posFwidth, normalFwidth);
    output.LinearZ = float4(linearZ, maxChangeZ, prevClipPos.z, packedNormalOS_asFloat);

    uint packedShadingNormal = EncodeNormal16x2(normal);
    output.SvgfCompact = float4(asfloat(packedShadingNormal), linearZ, maxChangeZ, 0.0f);
    return output;
}
