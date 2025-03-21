#define THREAD_GROUP_SIZE 16



struct TensorTextureConversionsConstants
{
    float2 resolution;   
    bool   nhwc;             
    uint   outputIdx;
    uint   inputIdx;
};
ConstantBuffer<TensorTextureConversionsConstants> TensorTextureConversionsPassCB : register(b1);

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void TensorToTextureCS( uint3 DTid : SV_DispatchThreadID )
{
    const float2 outputResolution = TensorTextureConversionsPassCB.resolution;
    if (DTid.x >= outputResolution.x || DTid.y >= outputResolution.y)
        return;

    Texture2D<float4> inputTexture  = ResourceDescriptorHeap[TensorTextureConversionsPassCB.outputIdx];
    RWBuffer<half> inputTensor = ResourceDescriptorHeap[TensorTextureConversionsPassCB.inputIdx];
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void TextureToTensorCS( uint3 DTid : SV_DispatchThreadID )
{
    const float2 inputResolution = TensorTextureConversionsPassCB.resolution;
    if (DTid.x >= inputResolution.x || DTid.y >= inputResolution.y)
        return;

    Texture2D<float4> inputTexture  = ResourceDescriptorHeap[TensorTextureConversionsPassCB.inputIdx];
    RWBuffer<half> outputTensor = ResourceDescriptorHeap[TensorTextureConversionsPassCB.outputIdx];

    uint index = DTid.y * inputResolution.x + DTid.x;
    float3 val = inputTexture[DTid.xy].xyz;
    outputTensor[index * 3] = val.x;
    outputTensor[index * 3 + 1] = val.y;
    outputTensor[index * 3 + 2] = val.z;
}