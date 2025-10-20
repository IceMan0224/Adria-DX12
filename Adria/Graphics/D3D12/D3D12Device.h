#pragma once
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"
#include "D3D12Fence.h"
#include "D3D12DescriptorHeap.h"
#include "D3D12Capabilities.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxRayTracingAS.h"

namespace adria
{
	class GfxPipelineState;
	class D3D12CommandQueue;
	class D3D12NsightAftermathGpuCrashTracker;
	struct DRED;

	class D3D12Device final : public GfxDevice
	{
	public:
		explicit D3D12Device(Window* window);
		ADRIA_NONCOPYABLE(D3D12Device)
		ADRIA_DEFAULT_MOVABLE(D3D12Device)
		~D3D12Device();

		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual GfxTexture* GetBackbuffer() const override;
		virtual Uint32 GetBackbufferIndex() const override;
		virtual Uint32 GetFrameIndex() const override;
		virtual constexpr Uint32 GetBackbufferCount() const override { return GFX_BACKBUFFER_COUNT; }

		virtual void Update() override;
		virtual void BeginFrame() override;
		virtual void EndFrame() override;
		virtual Bool IsFirstFrame() override { return first_frame; }

		virtual void SetRenderingNotStarted() override;
		virtual void InitGlobalResourceBindings(Uint32 max_resources) override;

		virtual void* GetNative() const override { return device.Get(); }
		virtual void* GetWindowHandle() const override { return hwnd; }

		virtual GfxCapabilities const& GetCapabilities() const override { return device_capabilities; }
		virtual GfxVendor GetVendor() const override { return vendor; }
		virtual GfxBackend GetBackend() const override { return GfxBackend::D3D12; }

		virtual GfxNsightPerfManager* GetNsightPerfManager() const override { return nsight_perf_manager.get(); }

		virtual void WaitForGPU() override;
		virtual GfxCommandQueue* GetCommandQueue(GfxCommandListType type) const override;
		virtual GfxFence& GetFence(GfxCommandListType type) override;
		virtual Uint64 GetFenceValue(GfxCommandListType type) const override;
		virtual void SetFenceValue(GfxCommandListType type, Uint64 value) override;

		virtual GfxCommandList* GetCommandList(GfxCommandListType type) const override;
		virtual GfxCommandList* GetLatestCommandList(GfxCommandListType type) const override;
		virtual GfxCommandList* AllocateCommandList(GfxCommandListType type) const override;
		virtual void FreeCommandList(GfxCommandList*, GfxCommandListType type) override;

		virtual GfxLinearDynamicAllocator* GetDynamicAllocator() const override;

		virtual GfxBindlessTable AllocateBindlessTable(Uint32 count, GfxDescriptorType type = GfxDescriptorType::CBV_SRV_UAV) override;
		virtual void UpdateBindlessTable(GfxBindlessTable table, std::span<GfxDescriptor const> src_descriptors) override;
		virtual void UpdateBindlessTable(GfxBindlessTable table, Uint32 table_offset, GfxDescriptor src_descriptor, Uint32 src_count) override;
		virtual void UpdateBindlessTables(std::vector<GfxBindlessTable> const& table, std::span<std::pair<GfxDescriptor, Uint32>> src_range_starts_and_size) override;
		virtual void FreeDescriptor(GfxDescriptor descriptor) override;

		virtual std::unique_ptr<GfxCommandList> CreateCommandList(GfxCommandListType type) override;
		virtual std::unique_ptr<D3D12DescriptorHeap> CreateDescriptorHeap(D3D12DescriptorHeapDesc const& desc) override;
		virtual std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc) override;
		virtual std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data) override;
		virtual std::unique_ptr<GfxTexture> CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer) override;
		virtual std::unique_ptr<GfxBuffer>  CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& initial_data) override;
		virtual std::unique_ptr<GfxBuffer>  CreateBuffer(GfxBufferDesc const& desc) override;

		virtual std::shared_ptr<GfxBuffer>  CreateBufferShared(GfxBufferDesc const& desc, GfxBufferData const& initial_data) override;
		virtual std::shared_ptr<GfxBuffer>  CreateBufferShared(GfxBufferDesc const& desc) override;

		virtual std::unique_ptr<GfxPipelineState> CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc) override;
		virtual std::unique_ptr<GfxPipelineState> CreateComputePipelineState(GfxComputePipelineStateDesc const& desc) override;
		virtual std::unique_ptr<GfxPipelineState> CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc) override;

		virtual std::unique_ptr<GfxFence> CreateFence(Char const* name) override;
		virtual std::unique_ptr<GfxQueryHeap> CreateQueryHeap(GfxQueryHeapDesc const& desc) override;
		virtual std::unique_ptr<GfxRayTracingTLAS> CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags) override;
		virtual std::unique_ptr<GfxRayTracingBLAS> CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags) override;
		virtual std::unique_ptr<GfxRayTracingPipeline> CreateRayTracingPipeline(GfxRayTracingPipelineDesc const& desc) override;

		virtual GfxDescriptor CreateBufferSRV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) override;
		virtual GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) override;
		virtual GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) override;
		virtual GfxDescriptor CreateTextureSRV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;
		virtual GfxDescriptor CreateTextureUAV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;
		virtual GfxDescriptor CreateTextureRTV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;
		virtual GfxDescriptor CreateTextureDSV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;

		virtual Uint64 GetLinearBufferSize(GfxTexture const* texture) const override;
		virtual Uint64 GetLinearBufferSize(GfxBuffer const* buffer) const override;

		virtual void GetTimestampFrequency(Uint64& frequency) const override;
		virtual GPUMemoryUsage GetMemoryUsage() const override;

		virtual GfxShadingRateInfo const& GetShadingRateInfo() const override { return shading_rate_info; }
		virtual void SetShadingRateInfo(GfxShadingRateInfo const& info) override { shading_rate_info = info; }

		IDXGIFactory4* GetFactory() const
		{
			return dxgi_factory.Get();
		}
		ID3D12Device5* GetD3D12Device() const
		{
			return device.Get();
		}
		IDMLDevice* GetDMLDevice() const
		{
			return dml_device.Get();
		}
		IDMLCommandRecorder* GetDMLCommandRecorder() const
		{
			return dml_command_recorder.Get();
		}
		ID3D12RootSignature* GetCommonRootSignature() const
		{
			return global_root_signature.Get();
		}
		D3D12MA::Allocator* GetD3D12Allocator() const
		{
			return allocator.get();
		}
		GfxOnlineDescriptorAllocator* GetDescriptorAllocator() const;

	private:
		void* hwnd;
		Uint32 width, height;
		Uint32 frame_index = 0;

		Ref<IDXGIFactory6> dxgi_factory = nullptr;
		Ref<ID3D12Device5> device = nullptr;
		D3D12Capabilities device_capabilities{};
		GfxVendor vendor = GfxVendor::Unknown;

		std::unique_ptr<GfxOnlineDescriptorAllocator> gpu_descriptor_allocator;
		std::array<std::unique_ptr<D3D12DescriptorAllocator>, (Uint64)GfxDescriptorType::Count> cpu_descriptor_allocators;

		std::unique_ptr<GfxSwapchain> swapchain;
		ReleasablePtr<D3D12MA::Allocator> allocator = nullptr;

		std::unique_ptr<D3D12CommandQueue> graphics_queue;
		std::unique_ptr<D3D12CommandQueue> compute_queue;
		std::unique_ptr<D3D12CommandQueue> copy_queue;

		D3D12Fence	 frame_fence;
		Uint64		 frame_fence_value = 1;
		Uint64       frame_fence_values[GFX_BACKBUFFER_COUNT];

		std::unique_ptr<GfxGraphicsCommandListPool> graphics_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		D3D12Fence graphics_fence;
		Uint64   graphics_fence_value = 0;

		std::unique_ptr<GfxComputeCommandListPool> compute_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		D3D12Fence compute_fence;
		Uint64   compute_fence_value = 0;

		std::unique_ptr<GfxCopyCommandListPool> copy_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		D3D12Fence copy_fence;
		Uint64   copy_fence_value = 0;

		D3D12Fence   wait_fence;
		Uint64       wait_fence_value = 1;

		D3D12Fence   release_fence;
		Uint64       release_queue_fence_value = 1;
		struct ReleasableItem
		{
			std::unique_ptr<ReleasableObject> obj;
			Uint64 fence_value;

			ReleasableItem(ReleasableObject* obj, Uint64 fence_value) : obj(obj), fence_value(fence_value) {}
		};
		std::queue<ReleasableItem>  release_queue;

		Ref<ID3D12RootSignature> global_root_signature = nullptr;

		std::vector<std::unique_ptr<GfxLinearDynamicAllocator>> dynamic_allocators;
		std::unique_ptr<GfxLinearDynamicAllocator> dynamic_allocator_on_init;

		GfxShadingRateInfo shading_rate_info;

		std::unique_ptr<DRED> dred;
		Bool rendering_not_started = true;
		Bool first_frame = false;

		Bool pix_dll_loaded = false;

		std::unique_ptr<D3D12NsightAftermathGpuCrashTracker> nsight_aftermath;
		std::unique_ptr<GfxNsightPerfManager> nsight_perf_manager;

		Ref<IDMLDevice>              dml_device;
		Ref<IDMLCommandRecorder>     dml_command_recorder;

	private:
		virtual void AddToReleaseQueue_Internal(ReleasableObject* _obj) override;
		void ProcessReleaseQueue();

		void SetupOptions(Uint32& dxgi_factory_flags);
		void SetInfoQueue();
		void CreateCommonRootSignature();

		D3D12Descriptor AllocateDescriptorCPU(GfxDescriptorType type);
		void FreeDescriptorCPU(D3D12Descriptor descriptor, GfxDescriptorType type);
		D3D12Descriptor AllocateDescriptorsGPU(Uint32 count = 1);

		D3D12Descriptor CreateBufferViewImpl(GfxBuffer const* buffer, GfxSubresourceType view_type, GfxBufferDescriptorDesc const& view_desc, GfxBuffer const* uav_counter = nullptr);
		D3D12Descriptor CreateTextureViewImpl(GfxTexture const* texture, GfxSubresourceType view_type, GfxTextureDescriptorDesc const& view_desc);
	};
}