#pragma once
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCapabilities.h"
#include "Graphics/GfxCommandListPool.h"

#ifdef __OBJC__
    @protocol MTLDevice;
    @protocol MTLCommandQueue;
    @protocol MTLLibrary;
    @protocol MTLBuffer;
    @protocol CAMetalDrawable;
    @protocol MTLResidencySet;
    #define ID_POINTER(x) id<x>
    #define ID_TYPE id
#else
    #define ID_POINTER(x) void*
    #define ID_TYPE void*
#endif

namespace adria
{
    class MetalSwapchain;
    class MetalTexture;
    class MetalArgumentBuffer;
    class MetalCommandList;
    class GfxLinearDynamicAllocator;

    struct MetalCapabilities : GfxCapabilities
    {
        virtual Bool Initialize(GfxDevice* gfx) override;
    };

    class MetalDevice : public GfxDevice
    {
    public:
        explicit MetalDevice(Window* window);
        ~MetalDevice() override;

        void OnResize(Uint32 w, Uint32 h) override;
        GfxTexture* GetBackbuffer() const override;
        Uint32 GetBackbufferIndex() const override;
        Uint32 GetFrameIndex() const override;
        Uint32 GetBackbufferCount() const override { return 2; }

        void SetRenderingNotStarted() override;

        void Update() override {}
        void BeginFrame() override;
        void EndFrame() override;
        Bool IsFirstFrame() override { return first_frame; }

        void* GetNative() const override;
        void* GetWindowHandle() const override;

        GfxCapabilities const& GetCapabilities() const override;
        GfxVendor GetVendor() const override { return GfxVendor::Apple; }
        GfxBackend GetBackend() const override { return GfxBackend::Metal; }

        GfxNsightPerfManager* GetNsightPerfManager() const override { return nullptr; }

        void WaitForGPU() override {}
        GfxCommandQueue* GetCommandQueue(GfxCommandListType type) const override { return nullptr; }
        GfxFence& GetFence(GfxCommandListType type) override;
        Uint64 GetFenceValue(GfxCommandListType type) const override { return 0; }
        void SetFenceValue(GfxCommandListType type, Uint64 value) override {}

        GfxCommandList* GetCommandList(GfxCommandListType type) const override;
        GfxCommandList* GetLatestCommandList(GfxCommandListType type) const override;
        GfxCommandList* AllocateCommandList(GfxCommandListType type) const override;
        void FreeCommandList(GfxCommandList*, GfxCommandListType type) override;

        GfxLinearDynamicAllocator* GetDynamicAllocator() const override;

        GfxBindlessTable AllocatePersistentBindlessTable(Uint32 count, GfxDescriptorType type = GfxDescriptorType::CBV_SRV_UAV) override;
        GfxBindlessTable AllocateBindlessTable(Uint32 count, GfxDescriptorType type = GfxDescriptorType::CBV_SRV_UAV) override;
        void UpdateBindlessTable(GfxBindlessTable table, std::span<GfxDescriptor const> src_descriptors) override;
        void UpdateBindlessTable(GfxBindlessTable table, Uint32 table_offset, GfxDescriptor src_descriptor, Uint32 src_count = 1) override;
        void UpdateBindlessTables(std::vector<GfxBindlessTable> const& table, std::span<std::pair<GfxDescriptor, Uint32>> src_range_starts_and_size) override;
        void FreeDescriptor(GfxDescriptor descriptor) override {}

        std::unique_ptr<GfxCommandList> CreateCommandList(GfxCommandListType type) override;
        std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc) override;
        std::unique_ptr<GfxTexture> CreateTexture(GfxTextureDesc const& desc, GfxTextureData const& data) override;
        std::unique_ptr<GfxTexture> CreateBackbufferTexture(GfxTextureDesc const& desc, void* backbuffer) override;
        std::unique_ptr<GfxBuffer> CreateBuffer(GfxBufferDesc const& desc, GfxBufferData const& initial_data) override;
        std::unique_ptr<GfxBuffer> CreateBuffer(GfxBufferDesc const& desc) override;

        std::shared_ptr<GfxBuffer> CreateBufferShared(GfxBufferDesc const& desc, GfxBufferData const& initial_data) override;
        std::shared_ptr<GfxBuffer> CreateBufferShared(GfxBufferDesc const& desc) override;

        std::unique_ptr<GfxPipelineState> CreateGraphicsPipelineState(GfxGraphicsPipelineStateDesc const& desc) override;
        std::unique_ptr<GfxPipelineState> CreateComputePipelineState(GfxComputePipelineStateDesc const& desc) override;
        std::unique_ptr<GfxPipelineState> CreateMeshShaderPipelineState(GfxMeshShaderPipelineStateDesc const& desc) override;
        std::unique_ptr<GfxFence> CreateFence(Char const* name) override;
        std::unique_ptr<GfxQueryHeap> CreateQueryHeap(GfxQueryHeapDesc const& desc) override { return nullptr; }
        std::unique_ptr<GfxRayTracingTLAS> CreateRayTracingTLAS(std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags) override;
        std::unique_ptr<GfxRayTracingBLAS> CreateRayTracingBLAS(std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags) override;
        std::unique_ptr<GfxRayTracingPipeline> CreateRayTracingPipeline(GfxRayTracingPipelineDesc const& desc) override;

        GfxDescriptor CreateBufferSRV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) override;
        GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) override;
        GfxDescriptor CreateBufferUAV(GfxBuffer const*, GfxBuffer const*, GfxBufferDescriptorDesc const* = nullptr) override;
        GfxDescriptor CreateTextureSRV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;
        GfxDescriptor CreateTextureUAV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;
        GfxDescriptor CreateTextureRTV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;
        GfxDescriptor CreateTextureDSV(GfxTexture const*, GfxTextureDescriptorDesc const* = nullptr) override;

        Uint64 GetLinearBufferSize(GfxTexture const* texture) const override { return 0; }
        Uint64 GetLinearBufferSize(GfxBuffer const* buffer) const override { return 0; }

        GfxShadingRateInfo const& GetShadingRateInfo() const override;
        void SetShadingRateInfo(GfxShadingRateInfo const& info) override {}

        void GetTimestampFrequency(Uint64& frequency) const override { frequency = 0; }
        GPUMemoryUsage GetMemoryUsage() const override { return {0, 0}; }

        MetalArgumentBuffer* GetArgumentBuffer() const { return argument_buffer.get(); }

#ifdef __OBJC__
        id<MTLDevice> GetMTLDevice() const { return device; }
        id<MTLCommandQueue> GetMTLCommandQueue() const { return command_queue; }
        id<MTLLibrary> GetMTLLibrary() const { return shader_library; }
        id<CAMetalDrawable> GetCurrentDrawable();

        void MakeResident(id<MTLBuffer> buffer);
        void MakeResident(id<MTLTexture> texture);
        void Evict(id<MTLBuffer> buffer);
        void Evict(id<MTLTexture> texture);

        struct BufferLookupResult
        {
            id<MTLBuffer> buffer;
            Uint64 offset;
        };

        void RegisterBuffer(id<MTLBuffer> buffer);
        void UnregisterBuffer(id<MTLBuffer> buffer);
        BufferLookupResult LookupBuffer(Uint64 gpu_address) const;
#endif

    private:
        void AddToReleaseQueue_Internal(ReleasableObject* _obj) override {}

        Window* window = nullptr;
        ID_POINTER(MTLDevice) device;
        ID_POINTER(MTLCommandQueue) command_queue;
        ID_POINTER(MTLLibrary) shader_library;
        ID_POINTER(MTLResidencySet) residency_set;
        Bool residency_dirty = false;

        std::unique_ptr<MetalSwapchain> swapchain;
        std::unique_ptr<MetalArgumentBuffer> argument_buffer;
        std::unique_ptr<GfxGraphicsCommandListPool> graphics_cmd_list_pool[GFX_BACKBUFFER_COUNT];
        std::unique_ptr<GfxComputeCommandListPool> compute_cmd_list_pool[GFX_BACKBUFFER_COUNT];
        std::unique_ptr<GfxCopyCommandListPool> copy_cmd_list_pool[GFX_BACKBUFFER_COUNT];
        std::vector<std::unique_ptr<GfxLinearDynamicAllocator>> dynamic_allocators;
        std::unique_ptr<GfxLinearDynamicAllocator> dynamic_allocator_on_init;
        Uint32 frame_index = 0;
        Uint32 backbuffer_index = 0;
        Bool first_frame = true;
        Bool rendering_not_started = true;
        MetalCapabilities capabilities;
        GfxShadingRateInfo shading_rate_info;
        GfxFence* dummy_fence = nullptr;

        struct EvictionEntry
        {
            ID_TYPE buffer_or_texture;
            Uint64 frame_id;
        };
        std::queue<EvictionEntry> eviction_queue;

        struct BufferEntry
        {
            ID_POINTER(MTLBuffer) buffer;
            Uint64 base_address;
            Uint64 size;
        };
        std::map<Uint64, BufferEntry> buffer_map; 
    };
}
