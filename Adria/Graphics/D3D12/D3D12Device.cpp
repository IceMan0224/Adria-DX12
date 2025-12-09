#include "D3D12Device.h"
#include "D3D12Swapchain.h"
#include "D3D12CommandQueue.h"
#include "D3D12CommandList.h"
#include "D3D12Texture.h"
#include "D3D12Buffer.h"
#include "D3D12Descriptor.h"
#include "D3D12DescriptorHeap.h"
#include "D3D12CommandSignature.h"
#include "D3D12QueryHeap.h"
#include "D3D12PipelineState.h"
#include "D3D12RayTracingAS.h"
#include "D3D12RayTracingPipeline.h"
#include "D3D12StateObject.h"
#include "D3D12DescriptorAllocator.h"
#include "D3D12RingDescriptorAllocator.h"
#include "D3D12NsightAftermathGpuCrashTracker.h"
#include "D3D12NsightPerfManager.h"
#include "D3D12Conversions.h"
#include "D3D12PIX.h"
#include "Graphics/GfxCommandListPool.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRenderDoc.h"
#include "d3dx12.h"
#include "Core/ConsoleManager.h"
#include "Core/CommandLineOptions.h"
#include "Core/Paths.h"
#include "Utilities/StringConversions.h"
#include "Platform/Window.h"
#include "tracy/Tracy.hpp"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }
extern "C" { __declspec(dllexport) extern LPCSTR D3D12SDKPath = ".\\"; }
extern "C" { __declspec(dllexport) extern UINT NvOptimusEnablement = true; }

namespace adria
{
	ADRIA_LOG_CHANNEL(Graphics);

	static constexpr Wchar const* DredBreadcrumbOpName(D3D12_AUTO_BREADCRUMB_OP);
	static constexpr Wchar const* DredAllocationName(D3D12_DRED_ALLOCATION_TYPE);
	static void LogDredInfo(ID3D12Device5*, ID3D12DeviceRemovedExtendedData1*);
	static void DeviceRemovedHandler(void*, BYTE);
	static void ReportLiveObjects();

	enum GfxVendorId : Uint32
	{
		GfxVendorId_AMD = 0x1002,
		GfxVendorId_Nvidia = 0x10de,
		GfxVendorId_Intel = 0x8086,
		GfxVendorId_Microsoft = 0x1414
	};
	static inline GfxVendor GetGfxVendor(Uint32 vendor_id)
	{
		switch (vendor_id)
		{
		case GfxVendorId_AMD: return GfxVendor::AMD;
		case GfxVendorId_Nvidia: return GfxVendor::Nvidia;
		case GfxVendorId_Intel: return GfxVendor::Intel;
		case GfxVendorId_Microsoft: return GfxVendor::Microsoft;
		}
		return GfxVendor::Unknown;
	}
	static inline Char const* GetGfxVendorName(GfxVendor gfx_vendor)
	{
		switch (gfx_vendor)
		{
		case GfxVendor::AMD: return "AMD";
		case GfxVendor::Nvidia: return "Nvidia";
		case GfxVendor::Intel: return "Intel";
		case GfxVendor::Microsoft: return "Microsoft";
		case GfxVendor::Apple:
		default:
			ADRIA_UNREACHABLE();
		}
		return "Unknown";
	}

	struct DRED
	{
		explicit DRED(GfxDevice* gfx)
		{
			dred_fence.Create(gfx, "DRED Fence");
			dred_wait_handle = CreateEventA(nullptr, false, false, nullptr);
			if (!dred_wait_handle)
			{
				return;
			}
			static_cast<ID3D12Fence*>(dred_fence.GetHandle())->SetEventOnCompletion(UINT64_MAX, dred_wait_handle);
			ADRIA_ASSERT(RegisterWaitForSingleObject(&dred_wait_handle, dred_wait_handle, DeviceRemovedHandler, (ID3D12Device*)gfx->GetNative(), INFINITE, 0));
		}
		~DRED()
		{
			dred_fence.Signal(UINT64_MAX);
			ADRIA_ASSERT(UnregisterWaitEx(dred_wait_handle, INVALID_HANDLE_VALUE));
			CloseHandle(dred_wait_handle);
		}

		D3D12Fence dred_fence;
		HANDLE   dred_wait_handle;
	};

	static TAutoConsoleVariable<Bool> VSync("rhi.VSync", false, "0: VSync is disabled. 1: VSync is enabled.");

	D3D12Device::D3D12Device(Window* window)
	{
		VSync->Set(CommandLineOptions::GetVSync());
		hwnd = window->Handle();
		width = window->Width();
		height = window->Height();

		HRESULT hr = E_FAIL;
		Uint32 dxgi_factory_flags = 0;
		SetupOptions(dxgi_factory_flags);
		D3D12_CHECK_CALL(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(dxgi_factory.GetAddressOf())));

		Ref<IDXGIAdapter4> adapter;
		Uint32 adapter_index = 0;
		ADRIA_LOG(INFO, "Available adapters:");
		DXGI_GPU_PREFERENCE gpu_preference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
		while (dxgi_factory->EnumAdapterByGpuPreference(adapter_index++, gpu_preference, IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())) == S_OK)
		{
			DXGI_ADAPTER_DESC3 desc{};
			adapter->GetDesc3(&desc);
			std::wstring adapter_wide_description(desc.Description);
			std::string adapter_description = ToString(adapter_wide_description);
			ADRIA_LOG(INFO, "\t%s - %f GB", adapter_description.c_str(), (Float)desc.DedicatedVideoMemory / (1 << 30));
		}
		dxgi_factory->EnumAdapterByGpuPreference(0, gpu_preference, IID_PPV_ARGS(adapter.GetAddressOf()));
		DXGI_ADAPTER_DESC3 desc{};
		adapter->GetDesc3(&desc);

		vendor = GetGfxVendor(desc.VendorId);
		ADRIA_ASSERT(vendor != GfxVendor::Unknown);
		Char const* vendor_name = GetGfxVendorName(vendor);
		ADRIA_LOG(INFO, "Vendor: %s", vendor_name);

		std::wstring adapter_wide_description(desc.Description);
		std::string adapter_description = ToString(adapter_wide_description);
		ADRIA_LOG(INFO, "GPU: %s", adapter_description.c_str());

		D3D_FEATURE_LEVEL feature_levels[] =
		{
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};

		if (CommandLineOptions::GetAftermath())
		{
			nsight_aftermath = std::make_unique<D3D12NsightAftermathGpuCrashTracker>(this);
		}

		D3D12_CHECK_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf())));
		D3D12_FEATURE_DATA_FEATURE_LEVELS caps{};
		caps.pFeatureLevelsRequested = feature_levels;
		caps.NumFeatureLevels = ARRAYSIZE(feature_levels);
		D3D12_CHECK_CALL(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &caps, sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS)));
		D3D12_CHECK_CALL(D3D12CreateDevice(adapter.Get(), caps.MaxSupportedFeatureLevel, IID_PPV_ARGS(device.ReleaseAndGetAddressOf())));

		if (!device_capabilities.Initialize(this))
		{
			ADRIA_DEBUGBREAK();
			std::exit(EXIT_FAILURE);
		}
		if (nsight_aftermath)
		{
			nsight_aftermath->Initialize();
		}

		D3D12MA::ALLOCATOR_DESC allocator_desc{};
		allocator_desc.pDevice = device.Get();
		allocator_desc.pAdapter = adapter.Get();
		D3D12MA::Allocator* _allocator = nullptr;
		D3D12_CHECK_CALL(D3D12MA::CreateAllocator(&allocator_desc, &_allocator));
		allocator.reset(_allocator);

		graphics_queue = std::make_unique<D3D12CommandQueue>(this, GfxCommandListType::Graphics, "Graphics Queue");
		compute_queue = std::make_unique<D3D12CommandQueue>(this, GfxCommandListType::Compute, "Compute Queue");
		copy_queue = std::make_unique<D3D12CommandQueue>(this, GfxCommandListType::Copy, "Copy Queue");

		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			graphics_cmd_list_pool[i] = std::make_unique<GfxGraphicsCommandListPool>(this);
			compute_cmd_list_pool[i] = std::make_unique<GfxComputeCommandListPool>(this);
			copy_cmd_list_pool[i] = std::make_unique<GfxCopyCommandListPool>(this);
		}

		for (Uint32 i = 0; i < (Uint32)GfxDescriptorType::Count; ++i)
		{
			D3D12DescriptorHeapDesc desc{};
			desc.descriptor_count = 1024;
			desc.shader_visible = false;
			desc.type = static_cast<GfxDescriptorType>(i);
			std::unique_ptr<D3D12DescriptorHeap> descriptor_heap = CreateDescriptorHeap(desc); 
			cpu_descriptor_allocators[i] = std::make_unique<D3D12DescriptorAllocator>(std::move(descriptor_heap));
		}

		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			dynamic_allocators.emplace_back(new GfxLinearDynamicAllocator(this, 1 << 20));
		}
		dynamic_allocator_on_init.reset(new GfxLinearDynamicAllocator(this, 1 << 30));

		GfxSwapchainDesc swapchain_desc{};
		swapchain_desc.width = width;
		swapchain_desc.height = height;
		swapchain_desc.fullscreen_windowed = true;
		swapchain_desc.backbuffer_format = GfxFormat::R8G8B8A8_UNORM;
		swapchain = std::make_unique<D3D12Swapchain>(this, swapchain_desc);

		D3D12DescriptorHeapDesc heap_desc{};
		heap_desc.descriptor_count = 32767;
		heap_desc.shader_visible = true;
		heap_desc.type = GfxDescriptorType::CBV_SRV_UAV;
		std::unique_ptr<D3D12DescriptorHeap> descriptor_heap = CreateDescriptorHeap(heap_desc);
		gpu_descriptor_allocator = std::make_unique<D3D12OnlineDescriptorAllocator>(std::move(descriptor_heap), 2048);

		frame_fence.Create(this, "Frame Fence");
		wait_fence.Create(this, "Wait Fence");
		release_fence.Create(this, "Release Fence");

		graphics_fence.Create(this, "Graphics Fence");
		copy_fence.Create(this, "Copy Fence");
		compute_fence.Create(this, "Compute Fence");

		for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
		{
			frame_fence_values[i] = 0;
		}

		SetInfoQueue();
		CreateCommonRootSignature();

		std::atexit(ReportLiveObjects);
		if (CommandLineOptions::GetDRED())
		{
			dred = std::make_unique<DRED>(this);
		}
		if (!CommandLineOptions::GetDebugDevice() && vendor == GfxVendor::Nvidia)
		{
			GfxNsightPerfMode perf_mode = GfxNsightPerfMode::None;
			if (CommandLineOptions::GetPerfReport())
			{
				perf_mode = GfxNsightPerfMode::HTMLReport;
			}
			if (CommandLineOptions::GetPerfHUD())
			{
				perf_mode = GfxNsightPerfMode::HUD;
			}
			nsight_perf_manager = std::make_unique<D3D12NsightPerfManager>(this, perf_mode);
		}

		D3D12_CHECK_CALL(DMLCreateDevice(device, CommandLineOptions::GetDebugDML() ? DML_CREATE_DEVICE_FLAG_DEBUG : DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(dml_device.GetAddressOf())));
		D3D12_CHECK_CALL(dml_device->CreateCommandRecorder(IID_PPV_ARGS(dml_command_recorder.GetAddressOf())));
	}
	D3D12Device::~D3D12Device()
	{
		WaitForGPU();
		ProcessReleaseQueue();
		frame_fence.Wait(frame_fence_values[swapchain->GetBackbufferIndex()]);
	}

	void D3D12Device::OnResize(Uint32 w, Uint32 h)
	{
		if ((width != w || height != h) && width > 0 && height > 0)
		{
			width = w;
			height = h;
			WaitForGPU();
			for (Uint32 i = 0; i < GFX_BACKBUFFER_COUNT; ++i)
			{
				frame_fence_values[i] = frame_fence_values[swapchain->GetBackbufferIndex()];
			}
			swapchain->OnResize(w, h);
		}
	}
	Uint32 D3D12Device::GetBackbufferIndex() const
	{
		return swapchain->GetBackbufferIndex();
	}

	Uint32 D3D12Device::GetFrameIndex() const { return frame_index; }

	GfxTexture* D3D12Device::GetBackbuffer() const
	{
		return swapchain->GetBackbuffer();
	}

	void D3D12Device::Update()
	{
		if (nsight_perf_manager)
		{
			nsight_perf_manager->Update();
		}
	}

	void D3D12Device::BeginFrame()
	{
		ZoneScopedN("GfxDevice::BeginFrame");
		if (rendering_not_started) [[unlikely]]
		{
			dynamic_allocator_on_init.reset();
			first_frame = true;
			rendering_not_started = false;
		}
		if (nsight_perf_manager)
		{
			nsight_perf_manager->BeginFrame();
		}

		Uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		frame_fence.Wait(frame_fence_values[backbuffer_index]);
		gpu_descriptor_allocator->ReleaseCompletedFrames(frame_fence_values[backbuffer_index]);

		graphics_cmd_list_pool[backbuffer_index]->BeginCmdLists();
		compute_cmd_list_pool[backbuffer_index]->BeginCmdLists();
		copy_cmd_list_pool[backbuffer_index]->BeginCmdLists();
		dynamic_allocators[backbuffer_index]->Clear();
	}
	void D3D12Device::EndFrame()
	{
		ZoneScopedN("GfxDevice::EndFrame");
		if (first_frame) [[unlikely]]
		{
			first_frame = false;
		}
		Uint32 backbuffer_index = swapchain->GetBackbufferIndex();

		graphics_cmd_list_pool[backbuffer_index]->EndCmdLists();
		compute_cmd_list_pool[backbuffer_index]->EndCmdLists();
		copy_cmd_list_pool[backbuffer_index]->EndCmdLists();

		ProcessReleaseQueue();
		ExecuteCommandListPool(graphics_queue.get(), *graphics_cmd_list_pool[backbuffer_index]);
		ExecuteCommandListPool(compute_queue.get(), *compute_cmd_list_pool[backbuffer_index]);
		ExecuteCommandListPool(copy_queue.get(), *copy_cmd_list_pool[backbuffer_index]);

		graphics_queue->Signal(frame_fence, frame_fence_value);
		frame_fence_values[backbuffer_index] = frame_fence_value;

		if (nsight_perf_manager)
		{
			nsight_perf_manager->EndFrame();
		}

		Bool present_successful = swapchain->Present(VSync.Get());
		if (!present_successful)
		{
			if (nsight_aftermath && nsight_aftermath->IsInitialized())
			{
				nsight_aftermath->HandleGpuCrash();
			}
			MessageBoxA(nullptr, "Swapchain present failed!", "GPU Crash", MB_OK);
			std::exit(EXIT_FAILURE);
		}
		GFX_RENDERDOC_ENDFRAME();

		gpu_descriptor_allocator->FinishCurrentFrame(frame_fence_value);
		++frame_fence_value;
		++frame_index;
	}

	void D3D12Device::WaitForGPU()
	{
		ZoneScopedN("GfxDevice::WaitForGPU");

		graphics_queue->Signal(wait_fence, wait_fence_value);
		wait_fence.Wait(wait_fence_value);
		wait_fence_value++;

		compute_queue->Signal(wait_fence, wait_fence_value);
		wait_fence.Wait(wait_fence_value);
		wait_fence_value++;

		copy_queue->Signal(wait_fence, wait_fence_value);
		wait_fence.Wait(wait_fence_value);
		wait_fence_value++;
	}
	GfxCommandQueue* D3D12Device::GetCommandQueue(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_queue.get();
		case GfxCommandListType::Compute:
			return compute_queue.get();
		case GfxCommandListType::Copy:
			return copy_queue.get();
		default:
			return graphics_queue.get();
		}
		ADRIA_UNREACHABLE();
	}

	GfxFence& D3D12Device::GetFence(GfxCommandListType type)
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_fence;
		case GfxCommandListType::Compute:  return compute_fence;
		case GfxCommandListType::Copy:	   return copy_fence;
		default: ADRIA_UNREACHABLE();
		}
		return graphics_fence;
	}

	Uint64 D3D12Device::GetFenceValue(GfxCommandListType type) const
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: return graphics_fence_value;
		case GfxCommandListType::Compute:  return compute_fence_value;
		case GfxCommandListType::Copy:	   return copy_fence_value;
		default: ADRIA_UNREACHABLE();
		}
		return graphics_fence_value;
	}

	void D3D12Device::SetFenceValue(GfxCommandListType type, Uint64 value)
	{
		switch (type)
		{
		case GfxCommandListType::Graphics: graphics_fence_value = value; break;
		case GfxCommandListType::Compute:  compute_fence_value = value; break;
		case GfxCommandListType::Copy:	   copy_fence_value = value; break;
		default: ADRIA_UNREACHABLE();
		}
	}

	GfxCommandList* D3D12Device::GetCommandList(GfxCommandListType type) const
	{
		Uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		default:
			return graphics_cmd_list_pool[backbuffer_index]->GetMainCmdList();
		}
		ADRIA_UNREACHABLE();
	}
	GfxCommandList* D3D12Device::GetLatestCommandList(GfxCommandListType type) const
	{
		Uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		default:
			return graphics_cmd_list_pool[backbuffer_index]->GetLatestCmdList();
		}
		ADRIA_UNREACHABLE();
	}
	GfxCommandList* D3D12Device::AllocateCommandList(GfxCommandListType type) const
	{
		Uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		default:
			return graphics_cmd_list_pool[backbuffer_index]->AllocateCmdList();
		}
		ADRIA_UNREACHABLE();
	}
	void D3D12Device::FreeCommandList(GfxCommandList* cmd_list, GfxCommandListType type)
	{
		Uint32 backbuffer_index = swapchain->GetBackbufferIndex();
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return graphics_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		case GfxCommandListType::Compute:
			return compute_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		case GfxCommandListType::Copy:
			return copy_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		default:
			return graphics_cmd_list_pool[backbuffer_index]->FreeCmdList(cmd_list);
		}
		ADRIA_UNREACHABLE();
	}

	D3D12Descriptor D3D12Device::AllocateCPUDescriptorImpl(GfxDescriptorType type)
	{
		return cpu_descriptor_allocators[(Uint64)type]->AllocateDescriptor();
	}
	void D3D12Device::FreeCPUDescriptorImpl(D3D12Descriptor descriptor, GfxDescriptorType type)
	{
		cpu_descriptor_allocators[(Uint64)type]->FreeDescriptor(descriptor);
	}

	D3D12OnlineDescriptorAllocator* D3D12Device::GetDescriptorAllocator() const
	{
		return gpu_descriptor_allocator.get();
	}
	GfxLinearDynamicAllocator* D3D12Device::GetDynamicAllocator() const
	{
		return rendering_not_started ? dynamic_allocator_on_init.get() : dynamic_allocators[swapchain->GetBackbufferIndex()].get();
	}

    std::unique_ptr<GfxCommandList> D3D12Device::CreateCommandList(GfxCommandListType type)
	{
		return std::make_unique<D3D12CommandList>(this, type);
	}
	std::unique_ptr<D3D12DescriptorHeap> D3D12Device::CreateDescriptorHeap(D3D12DescriptorHeapDesc const& desc)
	{
		return std::make_unique<D3D12DescriptorHeap>(this, desc);
	}

	std::unique_ptr<GfxTexture> D3D12Device::CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer)
	{
		return std::make_unique<D3D12Texture>(this, desc, backbuffer);
	}
	std::unique_ptr<GfxTexture> D3D12Device::CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data)
	{
		return std::make_unique<D3D12Texture>(this, desc, data);
	}
	std::unique_ptr<GfxTexture> D3D12Device::CreateTexture(GfxTextureDesc const& desc)
	{
		return std::make_unique<D3D12Texture>(this, desc);
	}
	std::unique_ptr<GfxBuffer>  D3D12Device::CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& initial_data)
	{
		return std::make_unique<D3D12Buffer>(this, desc, initial_data);
	}
	std::unique_ptr<GfxBuffer>	D3D12Device::CreateBuffer(GfxBufferDesc const& desc)
	{
		return std::make_unique<D3D12Buffer>(this, desc);
	}

	std::shared_ptr<GfxBuffer>  D3D12Device::CreateBufferShared(GfxBufferDesc const& desc, GfxBufferData const& initial_data)
	{
		return std::make_shared<D3D12Buffer>(this, desc, initial_data);
	}
	std::shared_ptr<GfxBuffer>	D3D12Device::CreateBufferShared(GfxBufferDesc const& desc)
	{
		return std::make_shared<D3D12Buffer>(this, desc);
	}

	std::unique_ptr<GfxPipelineState>	D3D12Device::CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc)
	{
		return std::make_unique<D3D12PipelineState>(this, desc);
	}
	std::unique_ptr<GfxPipelineState>	D3D12Device::CreateComputePipelineState(GfxComputePipelineStateDesc const& desc)
	{
		return std::make_unique<D3D12PipelineState>(this, desc);
	}
	std::unique_ptr<GfxPipelineState> D3D12Device::CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc)
	{
		return std::make_unique<D3D12PipelineState>(this, desc);
	}

	std::unique_ptr<GfxFence> D3D12Device::CreateFence(Char const* name)
	{
		std::unique_ptr<GfxFence> fence = std::make_unique<D3D12Fence>();
		fence->Create(this, name);
		return fence;
	}

	std::unique_ptr<GfxQueryHeap> D3D12Device::CreateQueryHeap(GfxQueryHeapDesc const& desc)
	{
		return std::make_unique<D3D12QueryHeap>(this, desc);
	}

	std::unique_ptr<GfxRayTracingTLAS> D3D12Device::CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags)
	{
		return std::make_unique<D3D12RayTracingTLAS>(this, instances, flags);
	}
	std::unique_ptr<GfxRayTracingBLAS> D3D12Device::CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags)
	{
		return std::make_unique<D3D12RayTracingBLAS>(this, geometries, flags);
	}

	std::unique_ptr<GfxRayTracingPipeline> D3D12Device::CreateRayTracingPipeline(GfxRayTracingPipelineDesc const& desc)
	{
		ADRIA_ASSERT(!desc.libraries.empty());
		ADRIA_ASSERT(desc.max_payload_size > 0);
		ADRIA_ASSERT(desc.max_recursion_depth > 0);

		Uint64 estimated_subobjects = 0;
		estimated_subobjects += 1;
		estimated_subobjects += 1;
		estimated_subobjects += 1;
		estimated_subobjects += desc.libraries.size();
		estimated_subobjects += desc.hit_groups.size();
		estimated_subobjects += desc.local_root_signatures.size() * 2; 

		D3D12StateObjectBuilder builder(estimated_subobjects);

		D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
		pipeline_config.MaxTraceRecursionDepth = desc.max_recursion_depth;
		builder.AddSubObject(pipeline_config);

		D3D12_RAYTRACING_SHADER_CONFIG shader_config = {};
		shader_config.MaxPayloadSizeInBytes = desc.max_payload_size;
		shader_config.MaxAttributeSizeInBytes = desc.max_attribute_size;
		builder.AddSubObject(shader_config);

		D3D12_GLOBAL_ROOT_SIGNATURE d3d12_global_root_signature = {};
		d3d12_global_root_signature.pGlobalRootSignature = global_root_signature.Get();
		builder.AddSubObject(d3d12_global_root_signature);

		std::vector<std::vector<std::wstring>> all_export_names_wide;
		std::vector<std::vector<D3D12_EXPORT_DESC>> all_export_descs;
		all_export_names_wide.reserve(desc.libraries.size());
		all_export_descs.reserve(desc.libraries.size());

		for (Uint64 lib_idx = 0; lib_idx < desc.libraries.size(); ++lib_idx)
		{
			GfxRayTracingShaderLibrary const& lib = desc.libraries[lib_idx];
			ADRIA_ASSERT(lib.shader != nullptr);
			ADRIA_ASSERT(lib.shader->GetData() != nullptr);
			ADRIA_ASSERT(lib.shader->GetSize() > 0);

			D3D12_DXIL_LIBRARY_DESC library_desc = {};
			library_desc.DXILLibrary.pShaderBytecode = lib.shader->GetData();
			library_desc.DXILLibrary.BytecodeLength = lib.shader->GetSize();

			if (!lib.exports.empty())
			{
				all_export_names_wide.emplace_back();
				all_export_descs.emplace_back();
				std::vector<std::wstring>& export_names_wide = all_export_names_wide.back();
				std::vector<D3D12_EXPORT_DESC>& export_descs = all_export_descs.back();

				export_names_wide.reserve(lib.exports.size());
				export_descs.reserve(lib.exports.size());
				for (Uint64 export_idx = 0; export_idx < lib.exports.size(); ++export_idx)
				{
					std::string const& export_name = lib.exports[export_idx];
					ADRIA_ASSERT(!export_name.empty());
					export_names_wide.push_back(ToWideString(export_name));

					D3D12_EXPORT_DESC export_desc{};
					export_desc.Name = export_names_wide.back().c_str();
					export_desc.ExportToRename = nullptr;
					export_desc.Flags = D3D12_EXPORT_FLAG_NONE;
					export_descs.push_back(export_desc);
				}

				library_desc.NumExports = static_cast<UINT>(export_descs.size());
				library_desc.pExports = export_descs.data();
			}
			else
			{
				library_desc.NumExports = 0;
				library_desc.pExports = nullptr;
			}
			builder.AddSubObject(library_desc);
		}

		std::vector<std::wstring> hit_group_names_wide;
		std::vector<std::wstring> closest_hit_names_wide;
		std::vector<std::wstring> any_hit_names_wide;
		std::vector<std::wstring> intersection_names_wide;
		hit_group_names_wide.reserve(desc.hit_groups.size());
		closest_hit_names_wide.reserve(desc.hit_groups.size());
		any_hit_names_wide.reserve(desc.hit_groups.size());
		intersection_names_wide.reserve(desc.hit_groups.size());
		for (Uint64 hg_idx = 0; hg_idx < desc.hit_groups.size(); ++hg_idx)
		{
			GfxRayTracingHitGroup const& hit_group = desc.hit_groups[hg_idx];
			ADRIA_ASSERT(!hit_group.name.empty());

			D3D12_HIT_GROUP_DESC hit_group_desc{};
			hit_group_names_wide.push_back(ToWideString(hit_group.name));
			hit_group_desc.HitGroupExport = hit_group_names_wide.back().c_str();
			if (hit_group.intersection_shader.empty())
			{
				hit_group_desc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			}
			else
			{
				hit_group_desc.Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
			}

			if (!hit_group.closest_hit_shader.empty())
			{
				closest_hit_names_wide.push_back(ToWideString(hit_group.closest_hit_shader));
				hit_group_desc.ClosestHitShaderImport = closest_hit_names_wide.back().c_str();
			}
			else
			{
				hit_group_desc.ClosestHitShaderImport = nullptr;
			}

			if (!hit_group.any_hit_shader.empty())
			{
				any_hit_names_wide.push_back(ToWideString(hit_group.any_hit_shader));
				hit_group_desc.AnyHitShaderImport = any_hit_names_wide.back().c_str();
			}
			else
			{
				hit_group_desc.AnyHitShaderImport = nullptr;
			}

			if (!hit_group.intersection_shader.empty())
			{
				intersection_names_wide.push_back(ToWideString(hit_group.intersection_shader));
				hit_group_desc.IntersectionShaderImport = intersection_names_wide.back().c_str();
			}
			else
			{
				hit_group_desc.IntersectionShaderImport = nullptr;
			}

			builder.AddSubObject(hit_group_desc);
		}

		std::vector<std::vector<std::wstring>> local_rs_shader_names_wide;
		std::vector<std::vector<LPCWSTR>> local_rs_shader_name_ptrs;
		local_rs_shader_names_wide.reserve(desc.local_root_signatures.size());
		local_rs_shader_name_ptrs.reserve(desc.local_root_signatures.size());
		for (Uint64 local_rs_idx = 0; local_rs_idx < desc.local_root_signatures.size(); ++local_rs_idx)
		{
			GfxRayTracingLocalRootSignatureAssociation const& association = desc.local_root_signatures[local_rs_idx];
			ADRIA_ASSERT(!association.shader_names.empty());

			D3D12_LOCAL_ROOT_SIGNATURE local_root_signature{};
			local_root_signature.pLocalRootSignature = nullptr; // association.root_signature->GetNative();
			builder.AddSubObject(local_root_signature);

			Uint64 local_rs_subobject_index = builder.GetNumSubobjects() - 1;
			D3D12_STATE_SUBOBJECT const* local_rs_subobject = builder.GetSubobject(static_cast<Uint32>(local_rs_subobject_index));
			ADRIA_ASSERT(local_rs_subobject != nullptr);

			local_rs_shader_names_wide.emplace_back();
			local_rs_shader_name_ptrs.emplace_back();
			std::vector<std::wstring>& shader_names_wide = local_rs_shader_names_wide.back();
			std::vector<LPCWSTR>& shader_name_ptrs = local_rs_shader_name_ptrs.back();
			shader_names_wide.reserve(association.shader_names.size());
			shader_name_ptrs.reserve(association.shader_names.size());
			for (Uint64 name_idx = 0; name_idx < association.shader_names.size(); ++name_idx)
			{
				std::string const& shader_name = association.shader_names[name_idx];
				ADRIA_ASSERT(!shader_name.empty());
				shader_names_wide.push_back(ToWideString(shader_name));
				shader_name_ptrs.push_back(shader_names_wide.back().c_str());
			}

			D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION subobject_association{};
			subobject_association.pSubobjectToAssociate = local_rs_subobject;
			subobject_association.NumExports = static_cast<UINT>(shader_name_ptrs.size());
			subobject_association.pExports = shader_name_ptrs.data();
			builder.AddSubObject(subobject_association);
		}

		D3D12StateObject* state_object = builder.CreateStateObject(this, D3D12StateObjectType::RayTracingPipeline);
		ADRIA_ASSERT(state_object != nullptr);
		ADRIA_ASSERT(state_object->IsValid());

		ID3D12StateObject* d3d12_state_object = static_cast<ID3D12StateObject*>(state_object->GetNative());
		ADRIA_ASSERT(d3d12_state_object != nullptr);
		d3d12_state_object->AddRef(); 
		delete state_object; 
		return std::make_unique<D3D12RayTracingPipeline>(d3d12_state_object);
	}

	GfxDescriptor D3D12Device::CreateBufferSRV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
	{
		GfxBufferDescriptorDesc _desc = desc ? *desc : GfxBufferDescriptorDesc{};
		return EncodeFromD3D12Descriptor(CreateBufferViewImpl(buffer, GfxSubresourceType::SRV, _desc));
	}
	GfxDescriptor D3D12Device::CreateBufferUAV(GfxBuffer const* buffer, GfxBufferDescriptorDesc const* desc)
	{
		GfxBufferDescriptorDesc _desc = desc ? *desc : GfxBufferDescriptorDesc{};
		return EncodeFromD3D12Descriptor(CreateBufferViewImpl(buffer, GfxSubresourceType::UAV, _desc));
	}
	GfxDescriptor D3D12Device::CreateBufferUAV(GfxBuffer const* buffer, GfxBuffer const* counter, GfxBufferDescriptorDesc const* desc/*= nullptr*/)
	{
		GfxBufferDescriptorDesc _desc = desc ? *desc : GfxBufferDescriptorDesc{};
		return EncodeFromD3D12Descriptor(CreateBufferViewImpl(buffer, GfxSubresourceType::UAV, _desc, counter));
	}
	GfxDescriptor D3D12Device::CreateTextureSRV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return EncodeFromD3D12Descriptor(CreateTextureViewImpl(texture, GfxSubresourceType::SRV, _desc));
	}
	GfxDescriptor D3D12Device::CreateTextureUAV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return EncodeFromD3D12Descriptor(CreateTextureViewImpl(texture, GfxSubresourceType::UAV, _desc));
	}
	GfxDescriptor D3D12Device::CreateTextureRTV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return EncodeFromD3D12Descriptor(CreateTextureViewImpl(texture, GfxSubresourceType::RTV, _desc));
	}
	GfxDescriptor D3D12Device::CreateTextureDSV(GfxTexture const* texture, GfxTextureDescriptorDesc const* desc)
	{
		GfxTextureDescriptorDesc _desc = desc ? *desc : GfxTextureDescriptorDesc{};
		return EncodeFromD3D12Descriptor(CreateTextureViewImpl(texture, GfxSubresourceType::DSV, _desc));
	}

	Uint64 D3D12Device::GetLinearBufferSize(GfxTexture const* texture) const
	{
		ADRIA_ASSERT(texture);
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT texture_footprint{};
		GfxTextureDesc const& desc = texture->GetDesc();
		D3D12_RESOURCE_DESC d3d12_texture_desc = ((ID3D12Resource*)texture->GetNative())->GetDesc();
		Uint32 subresource_count = desc.mip_levels * desc.array_size;
		device->GetCopyableFootprints(&d3d12_texture_desc, 0, subresource_count, 0, &texture_footprint, nullptr, nullptr, nullptr);
		return texture_footprint.Footprint.RowPitch * texture_footprint.Footprint.Height;
	}
	Uint64 D3D12Device::GetLinearBufferSize(GfxBuffer const* buffer) const
	{
		ADRIA_ASSERT(buffer);
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT buffer_footprint{};
		GfxBufferDesc const& desc = buffer->GetDesc();
		D3D12_RESOURCE_DESC d3d12_texture_desc = ((ID3D12Resource*)buffer->GetNative())->GetDesc();
		device->GetCopyableFootprints(&d3d12_texture_desc, 0, 1, 0, &buffer_footprint, nullptr, nullptr, nullptr);
		return buffer_footprint.Footprint.RowPitch * buffer_footprint.Footprint.Height;
	}

	void D3D12Device::GetTimestampFrequency(Uint64& frequency) const
	{
		frequency = graphics_queue->GetTimestampFrequency();
	}

	GPUMemoryUsage D3D12Device::GetMemoryUsage() const
	{
		GPUMemoryUsage gpu_memory_usage{};
		D3D12MA::Budget budget;
		allocator->GetBudget(&budget, nullptr);
		gpu_memory_usage.budget = budget.BudgetBytes;
		gpu_memory_usage.usage = budget.UsageBytes;
		return gpu_memory_usage;
	}

	void D3D12Device::SetRenderingNotStarted()
	{
		rendering_not_started = true;
		dynamic_allocator_on_init.reset(new GfxLinearDynamicAllocator(this, 1 << 30));
	}

	void D3D12Device::AddToReleaseQueue_Internal(ReleasableObject* _obj)
	{
		release_queue.emplace(_obj, release_queue_fence_value);
	}
	void D3D12Device::ProcessReleaseQueue()
	{
		while (!release_queue.empty())
		{
			if (!release_fence.IsCompleted(release_queue.front().fence_value))
			{
				break;
			}
			release_queue.pop();
		}
		graphics_queue->Signal(release_fence, release_queue_fence_value);
		++release_queue_fence_value;
	}
	void D3D12Device::SetInfoQueue()
	{
		Ref<ID3D12InfoQueue> info_queue;
		if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(info_queue.GetAddressOf()))))
		{
			D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			D3D12_MESSAGE_ID DenyIds[] =
			{
				D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,
				D3D12_MESSAGE_ID_COMMAND_ALLOCATOR_SYNC,
				D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED
			};

			D3D12_INFO_QUEUE_FILTER NewFilter{};
			NewFilter.DenyList.NumCategories = 0;
			NewFilter.DenyList.pCategoryList = NULL;
			NewFilter.DenyList.NumSeverities = ARRAYSIZE(Severities);
			NewFilter.DenyList.pSeverityList = Severities;
			NewFilter.DenyList.NumIDs = ARRAYSIZE(DenyIds);
			NewFilter.DenyList.pIDList = DenyIds;

			D3D12_CHECK_CALL(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false));
			D3D12_CHECK_CALL(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
			D3D12_CHECK_CALL(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
			info_queue->PushStorageFilter(&NewFilter);

			Ref<ID3D12InfoQueue1> info_queue1;
			info_queue.As(&info_queue1);
			if (info_queue1)
			{
				auto MessageCallback = [](
					D3D12_MESSAGE_CATEGORY Category,
					D3D12_MESSAGE_SEVERITY Severity,
					D3D12_MESSAGE_ID ID,
					LPCSTR pDescription,
					void* pContext)
					{
						ADRIA_LOG(WARNING, "D3D12 Validation Layer: %s", pDescription);
					};
				DWORD callbackCookie = 0;
				D3D12_CHECK_CALL(info_queue1->RegisterMessageCallback(MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &callbackCookie));
			}
		}
	}
	void D3D12Device::SetupOptions(Uint32& dxgi_factory_flags)
	{
		if (CommandLineOptions::GetAftermath())
		{
			return;
		}
		if (CommandLineOptions::GetDebugDevice())
		{
			Ref<ID3D12Debug> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_controller.GetAddressOf()))))
			{
				debug_controller->EnableDebugLayer();
				dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 Debug Layer Enabled!");
#else
				ADRIA_LOG(WARNING, "D3D12 Debug Layer Enabled! (Non-debug build)");
#endif
			}
			else ADRIA_LOG(WARNING, "Debug Layer setup failed!");
		}
		if (CommandLineOptions::GetDRED())
		{
			Ref<ID3D12DeviceRemovedExtendedDataSettings1> dred_settings;
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(dred_settings.GetAddressOf()));
			if (SUCCEEDED(hr) && dred_settings != NULL)
			{
				dred_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
				dred_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
				dred_settings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 DRED Enabled!");
#else
				ADRIA_LOG(WARNING, "D3D12 DRED Enabled! (Non-Debug build)");
#endif
			}
			else ADRIA_LOG(WARNING, "DRED setup failed!");
		}
		if (CommandLineOptions::GetGpuValidation())
		{
			Ref<ID3D12Debug1> debug_controller = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_controller.GetAddressOf()))))
			{
				debug_controller->SetEnableGPUBasedValidation(true);
#if defined(_DEBUG)
				ADRIA_LOG(INFO, "D3D12 GPU Based Validation Enabled!");
#else
				ADRIA_LOG(WARNING, "D3D12 GPU Based Validation Enabled! (Release)");
#endif
			}
		}
		if (CommandLineOptions::GetPIX())
		{
			GFX_PIX_INIT();
		}
		else if (CommandLineOptions::GetRenderDoc())
		{
			GFX_RENDERDOC_INIT();
		}
	}
	void D3D12Device::CreateCommonRootSignature()
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
		feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
		{
			feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_ROOT_PARAMETER1 root_parameters[4] = {}; //14 DWORDS = 8 * 1 DWORD for root constants + 3 * 2 DWORDS for CBVs
		root_parameters[0].InitAsConstantBufferView(0);
		root_parameters[1].InitAsConstants(8, 1);
		root_parameters[2].InitAsConstantBufferView(2);
		root_parameters[3].InitAsConstantBufferView(3);

		D3D12_ROOT_SIGNATURE_FLAGS flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

		CD3DX12_STATIC_SAMPLER_DESC static_samplers[10] = {};
		static_samplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		static_samplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		static_samplers[2].Init(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		static_samplers[2].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;

		static_samplers[3].Init(3, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		static_samplers[4].Init(4, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		static_samplers[5].Init(5, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		static_samplers[5].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;

		static_samplers[6].Init(6, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0.0f, 16u, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);
		static_samplers[7].Init(7, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0.0f, 16u, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

		static_samplers[8].Init(8, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		static_samplers[9].Init(9, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
		desc.Init_1_1(ARRAYSIZE(root_parameters), root_parameters, ARRAYSIZE(static_samplers), static_samplers, flags);

		Ref<ID3DBlob> signature;
		Ref<ID3DBlob> error;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, signature.GetAddressOf(), error.GetAddressOf());
		D3D12_CHECK_CALL(hr);
		hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(global_root_signature.GetAddressOf()));
		D3D12_CHECK_CALL(hr);
	}


	ADRIA_NODISCARD GfxDescriptor D3D12Device::AllocatePersistentGPUDescriptor(GfxDescriptorType type)
	{
		ADRIA_TODO("Need a way to free persistent descriptors");
		static Uint32 next_persistent_index = 0;
		ADRIA_ASSERT_MSG(next_persistent_index + 1 <= gpu_descriptor_allocator->GetReservedSize(), "Out of persistent bindless slots!");
		return EncodeFromD3D12Descriptor(gpu_descriptor_allocator->GetDescriptor(next_persistent_index++));
	}
	ADRIA_NODISCARD GfxDescriptor D3D12Device::AllocateTransientGPUDescriptor(GfxDescriptorType type)
	{
		return EncodeFromD3D12Descriptor(GetDescriptorAllocator()->Allocate(1));
	}

	void D3D12Device::CopyDescriptors(GfxDescriptor dst_descriptor, std::span<GfxDescriptor const> src_descriptors)
	{
		if (!dst_descriptor.IsValid() || src_descriptors.empty())
		{
			return;
		}

		D3D12DescriptorHeap* gpu_heap = gpu_descriptor_allocator->GetHeap();
		D3D12_CPU_DESCRIPTOR_HANDLE dst_range_starts[] = { DecodeToD3D12CPUHandle(dst_descriptor) };
		UINT dst_range_sizes[] = { (UINT)src_descriptors.size() };

		Uint64 const num_src_descriptors = src_descriptors.size();
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_range_starts;
		src_range_starts.reserve(num_src_descriptors);
		for (GfxDescriptor const& opaque_src_descriptor : src_descriptors)
		{
			src_range_starts.push_back(DecodeToD3D12CPUHandle(opaque_src_descriptor));
		}

		std::vector<UINT> src_range_sizes(num_src_descriptors, 1);
		device->CopyDescriptors(
			1,
			dst_range_starts,
			dst_range_sizes,
			(UINT)num_src_descriptors,
			src_range_starts.data(),
			src_range_sizes.data(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}
	void D3D12Device::CopyDescriptors(GfxDescriptor dst_descriptor, GfxDescriptor src_descriptor, Uint32 src_count)
	{
		if (!dst_descriptor.IsValid() || !src_descriptor.IsValid())
		{
			return;
		}

		D3D12DescriptorHeap* gpu_heap = gpu_descriptor_allocator->GetHeap();

		D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = DecodeToD3D12CPUHandle(dst_descriptor);
		D3D12_CPU_DESCRIPTOR_HANDLE src_handle = DecodeToD3D12CPUHandle(src_descriptor);
		device->CopyDescriptorsSimple(
			src_count,
			dst_handle,
			src_handle,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}
	void D3D12Device::CopyDescriptors(std::span<std::pair<GfxDescriptor, Uint32>> const& dst_range_starts_and_size, std::span<std::pair<GfxDescriptor, Uint32>> src_range_starts_and_size)
	{
		if (dst_range_starts_and_size.empty() || src_range_starts_and_size.empty())
		{
			return;
		}

		D3D12DescriptorHeap* gpu_heap = gpu_descriptor_allocator->GetHeap();

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> dst_range_starts;
		std::vector<UINT> dst_range_sizes;
		dst_range_starts.reserve(dst_range_starts_and_size.size());
		dst_range_sizes.reserve(dst_range_starts_and_size.size());
		Uint64 total_dst_descriptors = 0;
		for (auto const& [range_start, range_size] : dst_range_starts_and_size)
		{
			ADRIA_ASSERT(range_start.IsValid());
			dst_range_starts.push_back(DecodeToD3D12CPUHandle(range_start));
			dst_range_sizes.push_back(range_size);
			total_dst_descriptors += range_size;
		}

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_range_starts;
		std::vector<UINT> src_range_sizes;
		src_range_starts.reserve(src_range_starts_and_size.size());
		src_range_sizes.reserve(src_range_starts_and_size.size());

		Uint64 total_src_descriptors = 0;
		for (auto const& [range_start, range_size] : src_range_starts_and_size)
		{
			ADRIA_ASSERT(range_start.IsValid());
			src_range_starts.push_back(DecodeToD3D12CPUHandle(range_start));
			src_range_sizes.push_back(range_size);
			total_src_descriptors += range_size;
		}

		ADRIA_ASSERT(total_dst_descriptors == total_src_descriptors && "Total number of source and destination descriptors must match!");
		device->CopyDescriptors(
			(UINT)dst_range_starts_and_size.size(),
			dst_range_starts.data(),
			dst_range_sizes.data(),
			(UINT)src_range_starts_and_size.size(),
			src_range_starts.data(),
			src_range_sizes.data(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}
	void D3D12Device::FreeCPUDescriptor(GfxDescriptor descriptor)
	{
		if (!descriptor.IsValid())
		{
			return;
		}

		D3D12Descriptor internal_desc = DecodeToD3D12Descriptor(descriptor);
		if (!internal_desc.parent_heap)
		{
			ADRIA_LOG(WARNING, "Attempting to free a descriptor with a null parent heap.");
			return;
		}

		GfxDescriptorType const descriptor_type = internal_desc.parent_heap->GetType();
		ADRIA_ASSERT(!internal_desc.parent_heap->IsShaderVisible() && "Cannot free a GPU descriptor handle directly!");
		if (internal_desc.parent_heap->IsShaderVisible())
		{
			return;
		}
		FreeCPUDescriptorImpl(internal_desc, descriptor_type);
	}
	Uint32 D3D12Device::GetBindlessDescriptorIndex(GfxDescriptor descriptor) const
	{
		D3D12Descriptor internal_desc = DecodeToD3D12Descriptor(descriptor);
		ADRIA_ASSERT(internal_desc.parent_heap == gpu_descriptor_allocator->GetHeap() && "Descriptor is not from the bindless descriptor heap!");
		return internal_desc.index;
	}

	D3D12Descriptor D3D12Device::CreateBufferViewImpl(GfxBuffer const* buffer, GfxSubresourceType view_type, GfxBufferDescriptorDesc const& view_desc, GfxBuffer const* uav_counter, Bool force_cpu_heap)
	{
		if (uav_counter)
		{
			ADRIA_ASSERT(view_type == GfxSubresourceType::UAV);
		}

		GfxBufferDesc desc = buffer->GetDesc();
		GfxFormat format = desc.format;

		D3D12Descriptor heap_descriptor{};
		if (force_cpu_heap)
		{
			heap_descriptor = AllocateCPUDescriptorImpl(GfxDescriptorType::CBV_SRV_UAV);
		}
		else if(buffer->IsPersistent())
		{
			heap_descriptor = DecodeToD3D12Descriptor(AllocatePersistentGPUDescriptor(GfxDescriptorType::CBV_SRV_UAV));
		}
		else
		{
			heap_descriptor = DecodeToD3D12Descriptor(AllocateTransientGPUDescriptor(GfxDescriptorType::CBV_SRV_UAV));
		}

		switch (view_type)
		{
		case GfxSubresourceType::SRV:
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			Bool is_accel_struct = false;
			if (format == GfxFormat::UNKNOWN)
			{
				if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferRaw))
				{
					srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					srv_desc.Buffer.FirstElement = (Uint32)view_desc.offset / sizeof(Uint32);
					srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
					srv_desc.Buffer.NumElements = (Uint32)std::min<Uint64>(view_desc.size, desc.size - view_desc.offset) / sizeof(Uint32);
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferStructured))
				{
					srv_desc.Format = DXGI_FORMAT_UNKNOWN;
					srv_desc.Buffer.FirstElement = (Uint32)view_desc.offset / desc.stride;
					srv_desc.Buffer.NumElements = (Uint32)std::min<Uint64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
					srv_desc.Buffer.StructureByteStride = desc.stride;
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::AccelStruct))
				{
					is_accel_struct = true;
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
					srv_desc.RaytracingAccelerationStructure.Location = buffer->GetGpuAddress();
				}
			}
			else
			{
				Uint32 stride = GetGfxFormatStride(format);
				srv_desc.Format = ToDXGIFormat(format);
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srv_desc.Buffer.FirstElement = view_desc.offset / stride;
				srv_desc.Buffer.NumElements = (Uint32)std::min<Uint64>(view_desc.size, desc.size - view_desc.offset) / stride;
			}
			device->CreateShaderResourceView(!is_accel_struct ? (ID3D12Resource*)buffer->GetNative() : nullptr, &srv_desc, ToD3D12CPUHandle(heap_descriptor));
		}
		break;
		case GfxSubresourceType::UAV:
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav_desc.Buffer.FirstElement = 0;

			if (format == GfxFormat::UNKNOWN)
			{
				if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferRaw))
				{
					uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
					uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
					uav_desc.Buffer.FirstElement = (Uint32)view_desc.offset / sizeof(Uint32);
					uav_desc.Buffer.NumElements = (Uint32)std::min<Uint64>(view_desc.size, desc.size - view_desc.offset) / sizeof(Uint32);
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::BufferStructured))
				{
					uav_desc.Format = DXGI_FORMAT_UNKNOWN;
					uav_desc.Buffer.FirstElement = (Uint32)view_desc.offset / desc.stride;
					uav_desc.Buffer.NumElements = (Uint32)std::min<Uint64>(view_desc.size, desc.size - view_desc.offset) / desc.stride;
					uav_desc.Buffer.StructureByteStride = desc.stride;
				}
				else if (HasAllFlags(desc.misc_flags, GfxBufferMiscFlag::IndirectArgs))
				{
					uav_desc.Format = DXGI_FORMAT_R32_UINT;
					uav_desc.Buffer.FirstElement = (Uint32)view_desc.offset / sizeof(Uint32);
					uav_desc.Buffer.NumElements = (Uint32)std::min<Uint64>(view_desc.size, desc.size - view_desc.offset) / sizeof(Uint32);
				}
			}
			else
			{
				Uint32 stride = GetGfxFormatStride(format);
				uav_desc.Format = ToDXGIFormat(format);
				uav_desc.Buffer.FirstElement = (Uint32)view_desc.offset / stride;
				uav_desc.Buffer.NumElements = (Uint32)std::min<Uint64>(view_desc.size, desc.size - view_desc.offset) / stride;
			}
			device->CreateUnorderedAccessView((ID3D12Resource*)buffer->GetNative(), uav_counter ? (ID3D12Resource*)uav_counter->GetNative() : nullptr, &uav_desc, ToD3D12CPUHandle(heap_descriptor));
		}
		break;
		case GfxSubresourceType::RTV:
		case GfxSubresourceType::DSV:
		default:
			ADRIA_ASSERT_MSG(false, "Buffer View can only be UAV or SRV!");
		}
		return heap_descriptor;
	}
	D3D12Descriptor D3D12Device::CreateTextureViewImpl(GfxTexture const* texture, GfxSubresourceType view_type, GfxTextureDescriptorDesc const& view_desc, Bool force_cpu_heap)
	{
		GfxTextureDesc desc = texture->GetDesc();
		GfxFormat format = desc.format;
		switch (view_type)
		{
		case GfxSubresourceType::SRV:
		{
			D3D12Descriptor heap_descriptor{};
			if (force_cpu_heap)
			{
				heap_descriptor = AllocateCPUDescriptorImpl(GfxDescriptorType::CBV_SRV_UAV);
			}
			else if (texture->IsPersistent())
			{
				heap_descriptor = DecodeToD3D12Descriptor(AllocatePersistentGPUDescriptor(GfxDescriptorType::CBV_SRV_UAV));
			}
			else
			{
				heap_descriptor = DecodeToD3D12Descriptor(AllocateTransientGPUDescriptor(GfxDescriptorType::CBV_SRV_UAV));
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
			srv_desc.Shader4ComponentMapping = view_desc.channel_mapping;
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
			case GfxFormat::D32_FLOAT:
				srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				srv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				srv_desc.Format = ToDXGIFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
					srv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					srv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					srv_desc.Texture1DArray.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture1DArray.MipLevels = view_desc.mip_count;
				}
				else
				{
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
					srv_desc.Texture1D.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture1D.MipLevels = view_desc.mip_count;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (HasAnyFlag(desc.misc_flags, GfxTextureMiscFlag::TextureCube))
					{
						if (desc.array_size > 6)
						{
							srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
							srv_desc.TextureCubeArray.First2DArrayFace = view_desc.first_slice;
							srv_desc.TextureCubeArray.NumCubes = std::min<Uint32>(desc.array_size, view_desc.slice_count) / 6;
							srv_desc.TextureCubeArray.MostDetailedMip = view_desc.first_mip;
							srv_desc.TextureCubeArray.MipLevels = view_desc.mip_count;
						}
						else
						{
							srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
							srv_desc.TextureCube.MostDetailedMip = view_desc.first_mip;
							srv_desc.TextureCube.MipLevels = view_desc.mip_count;
						}
					}
					else
					{
						//if (texture->desc.sample_count > 1)
						//{
						//	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
						//	srv_desc.Texture2DMSArray.FirstArraySlice = firstSlice;
						//	srv_desc.Texture2DMSArray.ArraySize = sliceCount;
						//}
						//else
						srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
						srv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						srv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						srv_desc.Texture2DArray.MostDetailedMip = view_desc.first_mip;
						srv_desc.Texture2DArray.MipLevels = view_desc.mip_count;
					}
				}
				else
				{
					//if (texture->desc.sample_count > 1)
					//{
					//	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
					//}
					//else
					srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srv_desc.Texture2D.MostDetailedMip = view_desc.first_mip;
					srv_desc.Texture2D.MipLevels = view_desc.mip_count;
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				srv_desc.Texture3D.MostDetailedMip = view_desc.first_mip;
				srv_desc.Texture3D.MipLevels = view_desc.mip_count;
			}

			if (texture->IsSRGB())
			{
				auto AdjustFormatSRGB = [](DXGI_FORMAT format)
					{
						switch (format)
						{
						case DXGI_FORMAT_B8G8R8A8_UNORM:		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
						case DXGI_FORMAT_R8G8B8A8_UNORM:		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
						case DXGI_FORMAT_BC1_UNORM:				return DXGI_FORMAT_BC1_UNORM_SRGB;
						case DXGI_FORMAT_BC2_UNORM:				return DXGI_FORMAT_BC2_UNORM_SRGB;
						case DXGI_FORMAT_BC3_UNORM:				return DXGI_FORMAT_BC3_UNORM_SRGB;
						case DXGI_FORMAT_BC7_UNORM:				return DXGI_FORMAT_BC7_UNORM_SRGB;
						};
						return format;
					};
				srv_desc.Format = AdjustFormatSRGB(srv_desc.Format);
			}

			device->CreateShaderResourceView((ID3D12Resource*)texture->GetNative(), &srv_desc, ToD3D12CPUHandle(heap_descriptor));
			return heap_descriptor;
		}
		break;
		case GfxSubresourceType::UAV:
		{
			D3D12Descriptor heap_descriptor{};
			if (force_cpu_heap)
			{
				heap_descriptor = AllocateCPUDescriptorImpl(GfxDescriptorType::CBV_SRV_UAV);
			}
			else if (texture->IsPersistent())
			{
				heap_descriptor = DecodeToD3D12Descriptor(AllocatePersistentGPUDescriptor(GfxDescriptorType::CBV_SRV_UAV));
			}
			else
			{
				heap_descriptor = DecodeToD3D12Descriptor(AllocateTransientGPUDescriptor(GfxDescriptorType::CBV_SRV_UAV));
			}

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				uav_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				uav_desc.Format = ToDXGIFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
					uav_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					uav_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					uav_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
					uav_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					uav_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
					uav_desc.Texture2DArray.ArraySize = view_desc.slice_count;
					uav_desc.Texture2DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					uav_desc.Texture2D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
				uav_desc.Texture3D.MipSlice = view_desc.first_mip;
				uav_desc.Texture3D.FirstWSlice = 0;
				uav_desc.Texture3D.WSize = -1;
			}

			device->CreateUnorderedAccessView((ID3D12Resource*)texture->GetNative(), nullptr, &uav_desc, ToD3D12CPUHandle(heap_descriptor));
			return heap_descriptor;
		}
		break;
		case GfxSubresourceType::RTV:
		{
			D3D12Descriptor descriptor = AllocateCPUDescriptorImpl(GfxDescriptorType::RTV);
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				rtv_desc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				rtv_desc.Format = ToDXGIFormat(format);
				break;
			}

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
					rtv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					rtv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					rtv_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
					rtv_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (desc.sample_count > 1)
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
						rtv_desc.Texture2DMSArray.FirstArraySlice = view_desc.first_slice;
						rtv_desc.Texture2DMSArray.ArraySize = view_desc.slice_count;
					}
					else
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
						rtv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						rtv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						rtv_desc.Texture2DArray.MipSlice = view_desc.first_mip;
					}
				}
				else
				{
					if (desc.sample_count > 1)
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
						rtv_desc.Texture2D.MipSlice = view_desc.first_mip;
					}
				}
			}
			else if (desc.type == GfxTextureType_3D)
			{
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
				rtv_desc.Texture3D.MipSlice = view_desc.first_mip;
				rtv_desc.Texture3D.FirstWSlice = 0;
				rtv_desc.Texture3D.WSize = -1;
			}
			device->CreateRenderTargetView((ID3D12Resource*)texture->GetNative(), &rtv_desc, ToD3D12CPUHandle(descriptor));
			return descriptor;
		}
		break;
		case GfxSubresourceType::DSV:
		{
			D3D12Descriptor descriptor = AllocateCPUDescriptorImpl(GfxDescriptorType::DSV);
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
			switch (format)
			{
			case GfxFormat::R16_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D16_UNORM;
				break;
			case GfxFormat::R32_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
				break;
			case GfxFormat::R24G8_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				break;
			case GfxFormat::R32G8X24_TYPELESS:
				dsv_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
				break;
			default:
				dsv_desc.Format = ToDXGIFormat(format);
				break;
			}
			if (view_desc.flags & GfxTextureDescriptorFlag_DepthReadOnly) dsv_desc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
			else dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

			if (desc.type == GfxTextureType_1D)
			{
				if (desc.array_size > 1)
				{
					dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
					dsv_desc.Texture1DArray.FirstArraySlice = view_desc.first_slice;
					dsv_desc.Texture1DArray.ArraySize = view_desc.slice_count;
					dsv_desc.Texture1DArray.MipSlice = view_desc.first_mip;
				}
				else
				{
					dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
					dsv_desc.Texture1D.MipSlice = view_desc.first_mip;
				}
			}
			else if (desc.type == GfxTextureType_2D)
			{
				if (desc.array_size > 1)
				{
					if (desc.sample_count > 1)
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
						dsv_desc.Texture2DMSArray.FirstArraySlice = view_desc.first_slice;
						dsv_desc.Texture2DMSArray.ArraySize = view_desc.slice_count;
					}
					else
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
						dsv_desc.Texture2DArray.FirstArraySlice = view_desc.first_slice;
						dsv_desc.Texture2DArray.ArraySize = view_desc.slice_count;
						dsv_desc.Texture2DArray.MipSlice = view_desc.first_mip;
					}
				}
				else
				{
					if (desc.sample_count > 1)
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
						dsv_desc.Texture2D.MipSlice = view_desc.first_mip;
					}
				}
			}

			device->CreateDepthStencilView((ID3D12Resource*)texture->GetNative(), &dsv_desc, ToD3D12CPUHandle(descriptor));
			return descriptor;
		}
		break;
		}
		ADRIA_UNREACHABLE();
	}

	constexpr Wchar const* DredBreadcrumbOpName(D3D12_AUTO_BREADCRUMB_OP op)
	{
		switch (op)
		{
		case D3D12_AUTO_BREADCRUMB_OP_SETMARKER: return L"Set marker";
		case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT: return L"Begin event";
		case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT: return L"End event";
		case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED: return L"Draw instanced";
		case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED: return L"Draw indexed instanced";
		case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT: return L"Execute indirect";
		case D3D12_AUTO_BREADCRUMB_OP_DISPATCH: return L"Dispatch";
		case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION: return L"Copy buffer region";
		case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION: return L"Copy texture region";
		case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE: return L"Copy resource";
		case D3D12_AUTO_BREADCRUMB_OP_COPYTILES: return L"Copy tiles";
		case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE: return L"Resolve subresource";
		case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW: return L"Clear render target view";
		case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW: return L"Clear unordered access view";
		case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW: return L"Clear depth stencil view";
		case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER: return L"Resource barrier";
		case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE: return L"Execute bundle";
		case D3D12_AUTO_BREADCRUMB_OP_PRESENT: return L"Present";
		case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA: return L"Resolve query data";
		case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION: return L"Begin submission";
		case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION: return L"End submission";
		case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME: return L"Decode frame";
		case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES: return L"Process frames";
		case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT: return L"Atomic copy buffer uint";
		case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64: return L"Atomic copy buffer uint64";
		case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION: return L"Resolve subresource region";
		case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE: return L"Write buffer immediate";
		case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1: return L"Decode frame 1";
		case D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION: return L"Set protected resource session";
		case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2: return L"Decode frame 2";
		case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1: return L"Process frames 1";
		case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE: return L"Build raytracing acceleration structure";
		case D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO: return L"Emit raytracing acceleration structure post build info";
		case D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE: return L"Copy raytracing acceleration structure";
		case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS: return L"Dispatch rays";
		case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND: return L"Initialize meta command";
		case D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND: return L"Execute meta command";
		case D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION: return L"Estimate motion";
		case D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP: return L"Resolve motion vector heap";
		case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1: return L"Set pipeline state 1";
		case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND: return L"Initialize extension command";
		case D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND: return L"Execute extension command";
		case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH: return L"Dispatch mesh";
		default: return L"Unknown";
		}
	}
	constexpr Wchar const* DredAllocationName(D3D12_DRED_ALLOCATION_TYPE type)
	{
		switch (type)
		{
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE: return L"Command queue";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR: return L"Command allocator";
		case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE: return L"Pipeline state";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST: return L"Command list";
		case D3D12_DRED_ALLOCATION_TYPE_FENCE: return L"Fence";
		case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP: return L"Descriptor heap";
		case D3D12_DRED_ALLOCATION_TYPE_HEAP: return L"Heap";
		case D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP: return L"Query heap";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE: return L"Command signature";
		case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY: return L"Pipeline library";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER: return L"Video decoder";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR: return L"Video processor";
		case D3D12_DRED_ALLOCATION_TYPE_RESOURCE: return L"Resource";
		case D3D12_DRED_ALLOCATION_TYPE_PASS: return L"Pass";
		case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION: return L"Crypto session";
		case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY: return L"Crypto session policy";
		case D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION: return L"Protected resource session";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP: return L"Video decoder heap";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL: return L"Command pool";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER: return L"Command recorder";
		case D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT: return L"State object";
		case D3D12_DRED_ALLOCATION_TYPE_METACOMMAND: return L"Meta command";
		case D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP: return L"Scheduling group";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR: return L"Video motion estimator";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP: return L"Video motion vector heap";
		case D3D12_DRED_ALLOCATION_TYPE_INVALID: return L"Invalid";
		default: return L"Unknown";
		}
	}
	void LogDredInfo(ID3D12Device5* device, ID3D12DeviceRemovedExtendedData1* dred)
	{
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 DredAutoBreadcrumbsOutput;
		if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&DredAutoBreadcrumbsOutput)))
		{
			ADRIA_LOG(DEBUG, "[DRED] Last tracked GPU operations:");
			std::map<Int32, Wchar const*> contextStrings;
			D3D12_AUTO_BREADCRUMB_NODE1 const* pNode = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
			while (pNode && pNode->pLastBreadcrumbValue)
			{
				Int32 lastCompletedOp = *pNode->pLastBreadcrumbValue;
				if (lastCompletedOp != (int)pNode->BreadcrumbCount && lastCompletedOp != 0)
				{
					Char const* cmd_list_name = "cmd_list";
					Char const* queue_name = "graphics queue";
					ADRIA_LOG(DEBUG, "[DRED] Commandlist \"%s\" on CommandQueue \"%s\", %d completed of %d", cmd_list_name, queue_name, lastCompletedOp, pNode->BreadcrumbCount);

					Int32 firstOp = std::max<Int32>(lastCompletedOp - 100, 0);
					Int32 lastOp = std::min<Int32>(lastCompletedOp + 20, Int32(pNode->BreadcrumbCount) - 1);

					contextStrings.clear();
					for (Uint32 breadcrumbContext = firstOp; breadcrumbContext < pNode->BreadcrumbContextsCount; ++breadcrumbContext)
					{
						const D3D12_DRED_BREADCRUMB_CONTEXT& context = pNode->pBreadcrumbContexts[breadcrumbContext];
						contextStrings[context.BreadcrumbIndex] = context.pContextString;
					}

					for (Int32 op = firstOp; op <= lastOp; ++op)
					{
						D3D12_AUTO_BREADCRUMB_OP breadcrumbOp = pNode->pCommandHistory[op];

						std::wstring context_string;
						auto it = contextStrings.find(op);
						if (it != contextStrings.end())
						{
							context_string = it->second;
						}

						Wchar const* opName = DredBreadcrumbOpName(breadcrumbOp);
						ADRIA_LOG(DEBUG, "\tOp: %d, %ls%ls%s", op, opName, context_string.c_str(), (op + 1 == lastCompletedOp) ? " - Last completed" : "");
					}
				}
				pNode = pNode->pNext;
			}
		}

		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&DredPageFaultOutput)))
		{
			ADRIA_LOG(DEBUG, "[DRED] PageFault at VA GPUAddress \"0x%llx\"", DredPageFaultOutput.PageFaultVA);

			D3D12_DRED_ALLOCATION_NODE const* pNode = DredPageFaultOutput.pHeadExistingAllocationNode;
			if (pNode)
			{
				ADRIA_LOG(DEBUG, "[DRED] Active objects with VA ranges that match the faulting VA:");
				while (pNode)
				{
					Wchar const* AllocTypeName = DredAllocationName(pNode->AllocationType);
					ADRIA_LOG(DEBUG, "\tName: %s (Type: %ls)", pNode->ObjectNameA, AllocTypeName);
					pNode = pNode->pNext;
				}
			}

			pNode = DredPageFaultOutput.pHeadRecentFreedAllocationNode;
			if (pNode)
			{
				ADRIA_LOG(DEBUG, "[DRED] Recent freed objects with VA ranges that match the faulting VA:");
				while (pNode)
				{
					Uint32 allocTypeIndex = pNode->AllocationType - D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE;
					Wchar const* AllocTypeName = DredAllocationName(pNode->AllocationType);
					ADRIA_LOG(DEBUG, "\tName: %s (Type: %ls)", pNode->ObjectNameA, AllocTypeName);
					pNode = pNode->pNext;
				}
			}
		}
	}
	void DeviceRemovedHandler(void* device, BYTE)
	{
		ID3D12Device5* device5 = static_cast<ID3D12Device5*>(device);
		HRESULT removed_reason = device5->GetDeviceRemovedReason();
		ADRIA_LOG(ERROR, "Device removed, reason code: %ld", removed_reason);

		Ref<ID3D12DeviceRemovedExtendedData1> dred;
		if (FAILED(device5->QueryInterface(IID_PPV_ARGS(dred.GetAddressOf()))))
		{
			ADRIA_LOG(ERROR, "Failed to get DRED interface");
		}
		else
		{
			LogDredInfo(device5, dred.Get());
		}
		std::exit(EXIT_FAILURE);
	}
	void ReportLiveObjects()
	{
		Ref<IDXGIDebug1> dxgi_debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgi_debug.GetAddressOf()))))
		{
			dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
	}
}

