#pragma once
#include "VulkanDefines.h"
#include "VulkanCapabilities.h"
#include "VulkanFence.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxPipelineState.h"
#include "Utilities/Releasable.h"
#include <vk_mem_alloc.h>
#include <unordered_map>
#include <map>

namespace adria
{
	struct VulkanBufferLookup
	{
		VkBuffer     buffer = VK_NULL_HANDLE;
		VkDeviceSize offset = 0;
	};

	class Window;
	class VulkanCommandQueue;
	class VulkanSwapchain;

	class VulkanDevice final : public GfxDevice
	{
		friend class VulkanCommandList;
		friend class VulkanBuffer;
		friend class VulkanTexture;
		friend class VulkanPipelineState;
		friend class VulkanSwapchain;

	public:
		explicit VulkanDevice(Window* window);
		ADRIA_NONCOPYABLE(VulkanDevice)
		ADRIA_NONMOVABLE(VulkanDevice)
		~VulkanDevice();

		virtual void OnResize(Uint32 w, Uint32 h) override;
		virtual GfxTexture* GetBackbuffer() const override;
		virtual Uint32 GetBackbufferIndex() const override;
		virtual Uint32 GetFrameIndex() const override { return frame_index; }
		virtual constexpr Uint32 GetBackbufferCount() const override { return GFX_BACKBUFFER_COUNT; }

		virtual void SetRenderingNotStarted() override { rendering_not_started = true; }
		virtual void Update() override;
		virtual void BeginFrame() override;
		virtual void EndFrame() override;
		virtual Bool IsFirstFrame() override { return first_frame; }

		virtual void* GetNative() const override     { return (void*)device; }
		virtual void* GetWindowHandle() const override { return window_handle; }

		virtual GfxCapabilities const& GetCapabilities() const override { return capabilities; }
		virtual GfxVendor GetVendor() const override  { return vendor; }
		virtual GfxBackend GetBackend() const override { return GfxBackend::Vulkan; }

		virtual GfxNsightPerfManager* GetNsightPerfManager() const override { return nullptr; }

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

		virtual void FreeDescriptor(GfxDescriptor descriptor) override;
		virtual Uint32 GetBindlessDescriptorIndex(GfxDescriptor descriptor) const override;

		virtual std::unique_ptr<GfxCommandList> CreateCommandList(GfxCommandListType type) override;
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
		virtual GfxDescriptor CreateRayTracingTLASSRV(GfxRayTracingTLAS const*) override;

		virtual Uint64 GetLinearBufferSize(GfxTexture const* texture) const override;
		virtual Uint64 GetLinearBufferSize(GfxBuffer const* buffer) const override;

		virtual GfxShadingRateInfo const& GetShadingRateInfo() const override { return shading_rate_info; }
		virtual void SetShadingRateInfo(GfxShadingRateInfo const& info) override { shading_rate_info = info; }

		virtual void GetTimestampFrequency(Uint64& frequency) const override;
		virtual GPUMemoryUsage GetMemoryUsage() const override;

		VkDevice           GetVkDevice()       const { return device; }
		VkPhysicalDevice   GetVkPhysicalDevice()const { return physical_device; }
		VkInstance         GetVkInstance()     const { return instance; }
		VmaAllocator       GetVmaAllocator()   const { return vma_allocator; }
		VkPipelineLayout   GetCommonPipelineLayout() const { return common_pipeline_layout; }
		VkDescriptorSet    GetBindlessDescriptorSet() const { return bindless_set; }
		VkDescriptorSet    GetSamplerDescriptorSet()  const { return sampler_set; }
		Uint32             GetQueueFamilyIndex(GfxCommandListType type) const;
		VulkanBufferLookup GetBufferFromAddress(VkDeviceAddress address) const;

		Bool IsRayTracingSupported() const { return rt_supported; }
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR const& GetRayTracingPipelineProperties() const { return rt_pipeline_props; }
		VkPhysicalDeviceAccelerationStructurePropertiesKHR const& GetAccelerationStructureProperties() const { return as_props; }

		void TransitionImageLayoutImmediate(VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout);

	private:
		void* window_handle = nullptr;
		Uint32 width = 0, height = 0;
		Uint32 frame_index = 0;
		Bool   first_frame         = true;
		Bool   rendering_not_started = true;

		VkInstance       instance        = VK_NULL_HANDLE;
		VkPhysicalDevice physical_device = VK_NULL_HANDLE;
		VkDevice         device          = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

		VulkanCapabilities capabilities{};
		GfxVendor          vendor = GfxVendor::Unknown;

		Bool rt_supported = false;
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR    rt_pipeline_props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
		VkPhysicalDeviceAccelerationStructurePropertiesKHR as_props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

		Uint32 graphics_family = UINT32_MAX;
		Uint32 compute_family  = UINT32_MAX;
		Uint32 copy_family     = UINT32_MAX;

		std::unique_ptr<VulkanCommandQueue> graphics_queue;
		std::unique_ptr<VulkanCommandQueue> compute_queue;
		std::unique_ptr<VulkanCommandQueue> copy_queue;

		std::unique_ptr<VulkanSwapchain> swapchain;
		VmaAllocator vma_allocator = VK_NULL_HANDLE;

		VkDescriptorPool      bindless_pool   = VK_NULL_HANDLE;
		VkDescriptorSetLayout bindless_layout = VK_NULL_HANDLE;
		VkDescriptorSet       bindless_set    = VK_NULL_HANDLE;
		std::atomic<Uint32>   next_bindless_index[VK_BINDLESS_BINDING_COUNT]{};
		std::vector<Uint32>   bindless_free_list[VK_BINDLESS_BINDING_COUNT];
		std::vector<std::pair<Uint32, Uint32>> pending_bindless_releases[GFX_BACKBUFFER_COUNT]; // (binding, index) per in-flight frame
		std::mutex            bindless_mutex;

		VkDescriptorPool      sampler_pool   = VK_NULL_HANDLE;
		VkDescriptorSetLayout sampler_layout = VK_NULL_HANDLE;
		VkDescriptorSet       sampler_set    = VK_NULL_HANDLE;
		std::array<VkSampler, 10> static_samplers{};

		VkPipelineLayout common_pipeline_layout = VK_NULL_HANDLE;

		std::unique_ptr<GfxGraphicsCommandListPool> graphics_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		std::unique_ptr<GfxComputeCommandListPool>  compute_cmd_list_pool[GFX_BACKBUFFER_COUNT];
		std::unique_ptr<GfxCopyCommandListPool>     copy_cmd_list_pool[GFX_BACKBUFFER_COUNT];

		VulkanFence graphics_fence, compute_fence, copy_fence;
		Uint64 graphics_fence_value = 0;
		Uint64 compute_fence_value  = 0;
		Uint64 copy_fence_value     = 0;

		VulkanFence frame_fence;
		Uint64      frame_fence_value = 1;
		Uint64      frame_fence_values[GFX_BACKBUFFER_COUNT]{};

		VulkanFence release_fence;
		Uint64      release_fence_value = 1;
		struct ReleasableItem
		{
			std::unique_ptr<ReleasableObject> obj;
			Uint64 fence_value;
			ReleasableItem(ReleasableObject* o, Uint64 v) : obj(o), fence_value(v) {}
		};
		std::queue<ReleasableItem> release_queue;

		std::vector<std::unique_ptr<GfxLinearDynamicAllocator>> dynamic_allocators;
		
		GfxShadingRateInfo shading_rate_info{};

		std::map<VkDeviceAddress, VkBuffer> bda_to_buffer;
		mutable std::mutex bda_map_mutex;

	private:
		virtual void AddToReleaseQueue_Internal(ReleasableObject* obj) override;
		void ProcessReleaseQueue();

		void CreateInstance();
		void SelectPhysicalDevice();
		void CreateLogicalDevice();
		void CreateVmaAllocator();
		void CreateBindlessDescriptors();
		void CreateStaticSamplers();
		void CreateCommonPipelineLayout();
		void CreateCommandListPools();
		void CreateDynamicAllocators();

		VkSurfaceKHR CreateSurface();

		Uint32 AllocateBindlessIndex(Uint32 binding);
		void   FreeBindlessIndex(Uint32 binding, Uint32 index);
		void   RegisterBuffer(VkDeviceAddress address, VkBuffer buffer);
		void   UnregisterBuffer(VkDeviceAddress address);

		GfxDescriptor MakeBindlessDescriptor(Uint32 binding, Uint32 index) const;
		GfxDescriptor MakeViewDescriptor(VkImageView view) const;
	};
}
