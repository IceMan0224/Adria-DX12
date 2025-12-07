#pragma once
#include "GfxCapabilities.h"
#include "GfxCommandList.h"
#include "GfxDescriptor.h"
#include "GfxPipelineStateFwd.h"
#include "GfxDefines.h"
#include "GfxShadingRate.h"
#include "GfxRayTracingAS.h"
#include "Utilities/Releasable.h"

namespace adria
{
	class Window;

	class GfxSwapchain;
	class GfxCommandList;
	class GfxCommandQueue;
	class GfxCommandListPool;
	class GfxGraphicsCommandListPool;
	class GfxComputeCommandListPool;
	class GfxCopyCommandListPool;
	class GfxFence;
	enum class GfxSubresourceType : Uint8;

	class GfxTexture;
	struct GfxTextureDesc;
	struct GfxTextureDescriptorDesc;
	struct GfxTextureSubData;
	struct GfxTextureData;

	class GfxBuffer;
	struct GfxBufferDesc;
	struct GfxBufferDescriptorDesc;
	struct GfxBufferData;

	class GfxQueryHeap;
	struct GfxQueryHeapDesc;

	struct GfxGraphicsPipelineStateDesc;
	struct GfxComputePipelineStateDesc;
	struct GfxMeshShaderPipelineStateDesc;
	class GfxPipelineState;
	class DrawIndirectSignature;
	class DrawIndexedIndirectSignature;
	class DispatchIndirectSignature;
	class DispatchMeshIndirectSignature;

	class GfxLinearDynamicAllocator;
	class D3D12DescriptorAllocator;
	template<Bool>
	class D3D12RingDescriptorAllocator;

	class GfxNsightAftermathGpuCrashTracker;
	class GfxNsightPerfManager;

	class GfxRayTracingPipeline;
	struct GfxRayTracingPipelineDesc;

	struct GPUMemoryUsage
	{
		Uint64 usage;
		Uint64 budget;
	};

	enum class GfxVendor : Uint8
	{
		AMD,
		Nvidia,
		Intel,
		Microsoft,
		Apple,
		Unknown
	};

	enum class GfxBackend : Uint8
	{
		D3D12,
		Metal,
		Vulkan,
		Unknown
	};

	class GfxDevice
	{
	public:
		virtual ~GfxDevice() = default;

		virtual void OnResize(Uint32 w, Uint32 h) = 0;
		virtual GfxTexture* GetBackbuffer() const = 0;
		virtual Uint32 GetBackbufferIndex() const = 0;
		virtual Uint32 GetFrameIndex() const = 0;
		virtual constexpr Uint32 GetBackbufferCount() const = 0;

		virtual void SetRenderingNotStarted() = 0;

		virtual void Update() = 0;
		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual Bool IsFirstFrame() = 0;

		virtual void* GetNative() const = 0;
		virtual void* GetWindowHandle() const = 0;

		virtual GfxCapabilities const& GetCapabilities() const = 0;
		virtual GfxVendor GetVendor() const = 0;
		virtual GfxBackend GetBackend() const = 0;

		virtual GfxNsightPerfManager* GetNsightPerfManager() const = 0;

		virtual void WaitForGPU() = 0;
		virtual GfxCommandQueue* GetCommandQueue(GfxCommandListType type) const = 0;
		virtual GfxFence& GetFence(GfxCommandListType type) = 0;
		virtual Uint64 GetFenceValue(GfxCommandListType type) const = 0;
		virtual void SetFenceValue(GfxCommandListType type, Uint64 value) = 0;

		virtual GfxCommandList* GetCommandList(GfxCommandListType type) const = 0;
		virtual GfxCommandList* GetLatestCommandList(GfxCommandListType type) const = 0;
		virtual GfxCommandList* AllocateCommandList(GfxCommandListType type) const = 0;
		virtual void FreeCommandList(GfxCommandList*, GfxCommandListType type) = 0;

		template<Releasable T>
		void AddToReleaseQueue(T* alloc)
		{
			AddToReleaseQueue_Internal(new ReleasableResource(alloc));
		}

		virtual GfxLinearDynamicAllocator* GetDynamicAllocator() const = 0;

		ADRIA_NODISCARD virtual GfxBindlessTable AllocatePersistentBindlessTable(Uint32 count, GfxDescriptorType type = GfxDescriptorType::CBV_SRV_UAV) = 0;
		ADRIA_NODISCARD virtual GfxBindlessTable AllocateBindlessTable(Uint32 count, GfxDescriptorType type = GfxDescriptorType::CBV_SRV_UAV) = 0;
		virtual void UpdateBindlessTable(GfxBindlessTable table, std::span<GfxDescriptor const> src_descriptors) = 0;
		virtual void UpdateBindlessTable(GfxBindlessTable table, Uint32 table_offset, GfxDescriptor src_descriptor, Uint32 src_count = 1) = 0;
		virtual void UpdateBindlessTables(std::vector<GfxBindlessTable> const& table, std::span<std::pair<GfxDescriptor, Uint32>> src_range_starts_and_size) = 0;
		virtual void FreeDescriptor(GfxDescriptor descriptor) = 0;
		virtual Uint32 GetBindlessDescriptorIndex(GfxDescriptor descriptor) const = 0;

		virtual std::unique_ptr<GfxCommandList> CreateCommandList(GfxCommandListType type) = 0;
		virtual std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc) = 0;
		virtual std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data) = 0;
		virtual std::unique_ptr<GfxTexture> CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer) = 0;
		virtual std::unique_ptr<GfxBuffer>  CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& initial_data) = 0;
		virtual std::unique_ptr<GfxBuffer>  CreateBuffer(GfxBufferDesc const& desc) = 0;

		virtual std::shared_ptr<GfxBuffer>  CreateBufferShared(GfxBufferDesc const& desc, GfxBufferData const& initial_data) = 0;
		virtual std::shared_ptr<GfxBuffer>  CreateBufferShared(GfxBufferDesc const& desc) = 0;

		virtual std::unique_ptr<GfxPipelineState> CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc) = 0;
		virtual std::unique_ptr<GfxPipelineState> CreateComputePipelineState(GfxComputePipelineStateDesc const& desc) = 0;
		virtual std::unique_ptr<GfxPipelineState> CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc) = 0;
		virtual std::unique_ptr<GfxFence> CreateFence(Char const* name) = 0;
		virtual std::unique_ptr<GfxQueryHeap> CreateQueryHeap(GfxQueryHeapDesc const& desc) = 0;
		virtual std::unique_ptr<GfxRayTracingTLAS> CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags) = 0;
		virtual std::unique_ptr<GfxRayTracingBLAS> CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags) = 0;
		virtual std::unique_ptr<GfxRayTracingPipeline> CreateRayTracingPipeline(GfxRayTracingPipelineDesc const& desc) = 0;

		virtual GfxDescriptor CreateBufferSRV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) = 0;
		virtual GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) = 0;
		virtual GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) = 0;
		virtual GfxDescriptor CreateTextureSRV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) = 0;
		virtual GfxDescriptor CreateTextureUAV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) = 0;
		virtual GfxDescriptor CreateTextureRTV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) = 0;
		virtual GfxDescriptor CreateTextureDSV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) = 0;

		virtual Uint64 GetLinearBufferSize(GfxTexture const* texture) const = 0;
		virtual Uint64 GetLinearBufferSize(GfxBuffer const* buffer) const = 0;

		virtual GfxShadingRateInfo const& GetShadingRateInfo() const = 0;
		virtual void SetShadingRateInfo(GfxShadingRateInfo const& info) = 0;

		virtual void GetTimestampFrequency(Uint64& frequency) const = 0;
		virtual GPUMemoryUsage GetMemoryUsage() const = 0;

		GfxCommandQueue* GetGraphicsCommandQueue() const
		{
			return GetCommandQueue(GfxCommandListType::Graphics);
		}
		GfxCommandQueue* GetComputeCommandQueue() const
		{
			return GetCommandQueue(GfxCommandListType::Compute);
		}
		GfxCommandQueue* GetCopyCommandQueue() const
		{
			return GetCommandQueue(GfxCommandListType::Copy);
		}

		GfxCommandList* GetGraphicsCommandList() const
		{
			return GetCommandList(GfxCommandListType::Graphics);
		}
		GfxCommandList* GetLatestGraphicsCommandList() const
		{
			return GetLatestCommandList(GfxCommandListType::Graphics);
		}
		GfxCommandList* AllocateGraphicsCommandList() const
		{
			return AllocateCommandList(GfxCommandListType::Graphics);
		}
		void FreeGraphicsCommandList(GfxCommandList* cmd_list)
		{
			FreeCommandList(cmd_list, GfxCommandListType::Graphics);
		}
		GfxCommandList* GetComputeCommandList() const
		{
			return GetCommandList(GfxCommandListType::Compute);
		}
		GfxCommandList* GetLatestComputeCommandList() const
		{
			return GetLatestCommandList(GfxCommandListType::Compute);
		}
		GfxCommandList* AllocateComputeCommandList() const
		{
			return AllocateCommandList(GfxCommandListType::Compute);
		}
		void FreeComputeCommandList(GfxCommandList* cmd_list)
		{
			FreeCommandList(cmd_list, GfxCommandListType::Compute);
		}
		GfxCommandList* GetCopyCommandList() const
		{
			return GetCommandList(GfxCommandListType::Copy);
		}
		GfxCommandList* GetLatestCopyCommandList() const
		{
			return GetLatestCommandList(GfxCommandListType::Copy);
		}
		GfxCommandList* AllocateCopyCommandList() const
		{
			return AllocateCommandList(GfxCommandListType::Copy);
		}
		void FreeCopyCommandList(GfxCommandList* cmd_list)
		{
			FreeCommandList(cmd_list, GfxCommandListType::Copy);
		}
		
		GfxFence& GetGraphicsFence() { return GetFence(GfxCommandListType::Graphics); }
		GfxFence& GetComputeFence()  { return GetFence(GfxCommandListType::Compute); }
		GfxFence& GetCopyFence()     { return GetFence(GfxCommandListType::Copy); }
		Uint64 GetGraphicsFenceValue() const { return GetFenceValue(GfxCommandListType::Graphics); }
		Uint64 GetComputeFenceValue() const { return GetFenceValue(GfxCommandListType::Compute); }
		Uint64 GetCopyFenceValue() const { return GetFenceValue(GfxCommandListType::Copy); }
		void SetGraphicsFenceValue(Uint64 value) { SetFenceValue(GfxCommandListType::Graphics, value); }
		void SetComputeFenceValue(Uint64 value) { SetFenceValue(GfxCommandListType::Graphics, value); }
		void SetCopyFenceValue(Uint64 value) { SetFenceValue(GfxCommandListType::Graphics, value); }

		std::unique_ptr<GfxGraphicsPipelineState> CreateManagedGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc);
		std::unique_ptr<GfxComputePipelineState> CreateManagedComputePipelineState(GfxComputePipelineStateDesc const& desc);
		std::unique_ptr<GfxMeshShaderPipelineState> CreateManagedMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc);

		ADRIA_NODISCARD GfxBindlessTable AllocateAndUpdateBindlessTable(GfxDescriptor src_descriptor, GfxDescriptorType type = GfxDescriptorType::CBV_SRV_UAV)
		{
			GfxBindlessTable table = AllocateBindlessTable(1);
			UpdateBindlessTable(table, 0, src_descriptor, 1);
			return table;
		}
		ADRIA_NODISCARD GfxBindlessTable AllocateAndUpdateBindlessTable(std::span<GfxDescriptor const> src_descriptors, GfxDescriptorType type = GfxDescriptorType::CBV_SRV_UAV)
		{
			GfxBindlessTable table = AllocateBindlessTable(static_cast<Uint32>(src_descriptors.size()));
			UpdateBindlessTable(table, src_descriptors);
			return table;
		}

	private:
		virtual void AddToReleaseQueue_Internal(ReleasableObject* _obj) = 0;
	};

	std::unique_ptr<GfxDevice> CreateGfxDevice(GfxBackend backend, Window* window);
}