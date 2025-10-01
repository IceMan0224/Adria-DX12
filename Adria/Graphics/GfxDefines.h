#pragma once

#define GFX_CHECK_HR(hr) if(FAILED(hr)) ADRIA_DEBUGBREAK();

#define GFX_BACKBUFFER_COUNT 3
#define GFX_MULTITHREADED 0
#define GFX_SHADER_PRINTF 0 //broken since the newest DXC (1.8): string literal arguments not allowed (previously was not working with /Od)
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

#if defined(ADRIA_PLATFORM_WINDOWS)
#define GFX_BACKEND_DX12 1
#elif defined(ADRIA_PLATFORM_LINUX)
#define GFX_BACKEND_VULKAN 1
#elif defined((ADRIA_PLATFORM_MACOS)
#define GFX_BACKEND_METAL 1
#endif

#ifndef GFX_BACKEND_DX12
#define GFX_BACKEND_DX12 0
#endif

#ifndef GFX_BACKEND_VULKAN
#define GFX_BACKEND_VULKAN 0
#endif

#ifndef GFX_BACKEND_METAL
#define GFX_BACKEND_METAL 0
#endif



