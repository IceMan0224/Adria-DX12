
struct RemodulatePassConstants
{
    uint denoised_lighting_idx;
    uint albedo_idx;
    uint specular_idx;
    uint output_idx;
};
ConstantBuffer<RemodulatePassConstants> RemodulatePassCB : register(b1);

[numthreads(16, 16, 1)]
void Remodulate_CS(uint3 DTid : SV_DispatchThreadID)
{
    Texture2D<float4> denoisedLighting = ResourceDescriptorHeap[RemodulatePassCB.denoised_lighting_idx];
    Texture2D<float4> albedo = ResourceDescriptorHeap[RemodulatePassCB.albedo_idx];
    Texture2D<float4> specular = ResourceDescriptorHeap[RemodulatePassCB.specular_idx];
    RWTexture2D<float4> output = ResourceDescriptorHeap[RemodulatePassCB.output_idx];
    
    float3 denoisedDiffuse = denoisedLighting.Load(int3(DTid.xy, 0)).rgb;
    
    float3 albedoColor = albedo.Load(int3(DTid.xy, 0)).rgb;
    float3 specularColor = specular.Load(int3(DTid.xy, 0)).rgb;
    
    float3 finalColor = denoisedDiffuse * albedoColor + specularColor;
    
    output[DTid.xy] = float4(finalColor, 1.0);
}