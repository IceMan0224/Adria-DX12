

cbuffer ConstantBufferCS
{
    uint Height;
    uint Width;
    bool Nhwc;
};

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}