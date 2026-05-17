#ifndef _GPU_ASSERT_
#define _GPU_ASSERT_

#include "CommonResources.hlsli"

enum GpuAssertType
{
	GpuAssertType_Invalid,
	GpuAssertType_Generic,
	GpuAssertType_IndexOutOfBounds
};

struct GpuAssertHeader
{
    uint Type;
	uint NumArgs;
};  

struct GpuAssertContext
{
    static const uint BufferSize = 256;
    static const uint BufferSizeInBytes = BufferSize * sizeof(uint);
    uint InternalBuffer[BufferSize];
    uint ByteCount;
    uint Type;
    uint ArgCount;

    void Init()
    {
        for(uint i = 0; i < BufferSize; ++i) InternalBuffer[i] = 0;
        ByteCount = 0;
        Type = GpuAssertType_Invalid;
        ArgCount = 0;
    }

    uint CurrBufferIndex()
    {
        return ByteCount / 4;
    }

    void AppendArg(uint arg)
    {
        InternalBuffer[CurrBufferIndex()] = arg;
        ArgCount += 1;
        ByteCount += sizeof(uint);
    }

    void Assert(uint type)
    {
        Type = type;
    }
    void Assert(uint type, uint arg)
    {
        Type = type;
        AppendArg(arg);
    }
    void Assert(uint type, uint arg0, uint arg1)
    {
        Type = type;
        AppendArg(arg0);
        AppendArg(arg1);
    }
    void Assert(uint type, uint arg0, uint arg1, uint arg2)
    {
        Type = type;
        AppendArg(arg0);
        AppendArg(arg1);
        AppendArg(arg2);
    }
    void Assert(uint type, uint arg0, uint arg1, uint arg2, uint arg3)
    {
        Type = type;
        AppendArg(arg0);
        AppendArg(arg1);
        AppendArg(arg2);
        AppendArg(arg3);
    }

    void Commit()
    {
        if(FrameCB.assertBufferIdx < 0) return;
        if(ByteCount < 2) return;  
        ByteCount = ((ByteCount + 3) / 4) * 4;

        RWByteAddressBuffer assertBuffer = ResourceDescriptorHeap[FrameCB.assertBufferIdx];
        const uint numBytesToWrite = ByteCount + sizeof(GpuAssertHeader);
        uint offset = 0;
        assertBuffer.InterlockedAdd(0, numBytesToWrite, offset);
        offset += sizeof(uint);

        GpuAssertHeader header;
        header.Type = Type;
        header.NumArgs = ArgCount;

        assertBuffer.Store<GpuAssertHeader>(offset, header);
        offset += sizeof(GpuAssertHeader);
        for(uint i = 0; i < ByteCount / 4; ++i)
            assertBuffer.Store(offset + (i * sizeof(uint)), InternalBuffer[i]);
    }
};

#define GpuAssert(condition, ...) \
do \
{   \
    if(!condition) \
    {              \
        GpuAssertContext gpuAssertContext;      \
        gpuAssertContext.Init();                \
        gpuAssertContext.Assert(GpuAssertType_Generic, __VA_ARGS__);    \
        gpuAssertContext.Commit();              \
    }                                           \
} while(0)

#define GpuAssertEx(condition, type, ...) \
do \
{   \
    if(!condition) \
    {              \
        GpuAssertContext gpuAssertContext;      \
        gpuAssertContext.Init();                \
        gpuAssertContext.Assert(type, __VA_ARGS__);    \
        gpuAssertContext.Commit();              \
    }                                           \
} while(0)

#endif