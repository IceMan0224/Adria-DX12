#pragma once

#define GFX_CHECK_HR(hr) if(FAILED(hr)) ADRIA_DEBUGBREAK();

#define GFX_BACKBUFFER_COUNT 3
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


