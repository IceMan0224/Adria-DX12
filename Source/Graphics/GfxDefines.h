#pragma once

#define GFX_MULTITHREADED 0
#define GFX_SHADER_PRINTF 0 //broken since DXC (1.8): string literal arguments not allowed (previously was not working with /Od)
#define GFX_SHADER_ASSERT 0
#define GFX_ASYNC_COMPUTE 1
#define USE_PIX

#if defined(_DEBUG) || defined(_PROFILE)
	#define GFX_PROFILING 1
#endif

#if GFX_PROFILING 
#define GFX_PROFILING_USE_TRACY 1
#define GFX_ENABLE_NV_PERF
#endif

namespace adria
{
	static constexpr Uint32 GFX_CONSTANT_BUFFER_DATA_ALIGNMENT = 256;
	static constexpr Uint32 GFX_BACKBUFFER_COUNT = 3;

	struct GfxDrawArguments
	{
		Uint32 VertexCountPerInstance;
		Uint32 InstanceCount;
		Uint32 StartVertexLocation;
		Uint32 StartInstanceLocation;
	};

	struct GfxDrawIndexedArguments
	{
		Uint32 IndexCountPerInstance;
		Uint32 InstanceCount;
		Uint32 StartIndexLocation;
		Int32  BaseVertexLocation;
		Uint32 StartInstanceLocation;
	};

	struct GfxDispatchArguments
	{
		Uint32 ThreadGroupCountX;
		Uint32 ThreadGroupCountY;
		Uint32 ThreadGroupCountZ;
	};

	struct GfxDispatchMeshArguments
	{
		Uint32 ThreadGroupCountX;
		Uint32 ThreadGroupCountY;
		Uint32 ThreadGroupCountZ;
	};
}
