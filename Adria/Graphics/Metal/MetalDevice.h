#pragma once
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCapabilities.h"
#include "Graphics/GfxCommandListPool.h"

struct IRDescriptorTableEntry;

#ifdef __OBJC__
    @protocol MTLDevice;
    @protocol MTLCommandQueue;
    @protocol MTLLibrary;
    @protocol MTLBuffer;
    @protocol MTLSamplerState;
    @protocol MTLComputePipelineState;
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
    class MetalCommandList;
    class GfxLinearDynamicAllocator;

    struct MetalCapabilities : GfxCapabilities
    {
        virtual Bool Initialize(GfxDevice* gfx) override;
    };

    enum class MetalClearPipeline : Uint8
    {
        ClearBufferUint,
        ClearBufferUint4,
        ClearBufferFloat,
        ClearBufferFloat4,
        ClearTexture1DFloat,
        ClearTexture1DUint,
        ClearTexture1DInt,
        ClearTexture1DArrayFloat,
        ClearTexture1DArrayUint,
        ClearTexture1DArrayInt,
        ClearTexture2DFloat,
        ClearTexture2DUint,
        ClearTexture2DInt,
        ClearTexture2DArrayFloat,
        ClearTexture2DArrayUint,
        ClearTexture2DArrayInt,
        ClearTexture3DFloat,
        ClearTexture3DUint,
        ClearTexture3DInt,
        Count
    };

    class MetalDevice : public GfxDevice
    {
        static constexpr Uint32 STATIC_SAMPLER_COUNT = 10;
        static constexpr Uint32 CLEAR_PIPELINE_COUNT = static_cast<Uint32>(MetalClearPipeline::Count);

    public:
        explicit MetalDevice(Window* window);
        ~MetalDevice() override;

        void OnResize(Uint32 w, Uint32 h) override;
        GfxTexture* GetBackbuffer() const override;
        Uint32 GetBackbufferIndex() const override;
        Uint32 GetFrameIndex() const override;
        Uint32 GetBackbufferCount() const override { return GFX_BACKBUFFER_COUNT; }

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

        void FreeCPUDescriptor(GfxDescriptor descriptor) override;
        Uint32 GetBindlessDescriptorIndex(GfxDescriptor descriptor) const override;

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
        std::unique_ptr<GfxQueryHeap> CreateQueryHeap(GfxQueryHeapDesc const& desc) override;
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

        void GetTimestampFrequency(Uint64& frequency) const override;
        GPUMemoryUsage GetMemoryUsage() const override { return {0, 0}; }

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

        Uint32 AllocateResourceDescriptor(IRDescriptorTableEntry** descriptor);
        Uint32 AllocatePersistentResourceDescriptor(IRDescriptorTableEntry** descriptor);
        void FreeResourceDescriptor(Uint32 index);
        id<MTLBuffer> GetResourceDescriptorBuffer() const;
        id<MTLBuffer> GetSamplerTableBuffer() const { return sampler_table_buffer; }
        Uint64 GetSamplerTableGpuAddress() const { return sampler_table_gpu_address; }
        id<MTLComputePipelineState> GetClearPipeline(MetalClearPipeline pipeline) const { return clear_pipelines[static_cast<Uint32>(pipeline)]; }
#endif

    private:
        Window* window = nullptr;
        ID_POINTER(MTLDevice) device;
        ID_POINTER(MTLCommandQueue) command_queue;
        ID_POINTER(MTLLibrary) shader_library;
        ID_POINTER(MTLResidencySet) residency_set;
        ID_POINTER(MTLBuffer) sampler_table_buffer;
        ID_POINTER(MTLSamplerState) static_samplers[STATIC_SAMPLER_COUNT] = {};
        Uint64 sampler_table_gpu_address = 0;
        Bool residency_dirty = false;

        ID_POINTER(MTLLibrary) clear_library;
        ID_POINTER(MTLComputePipelineState) clear_pipelines[CLEAR_PIPELINE_COUNT] = {};


        std::unique_ptr<MetalSwapchain> swapchain;
        std::unique_ptr<class MetalRingDescriptorAllocator> resource_descriptor_allocator;
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

    private:
        void AddToReleaseQueue_Internal(ReleasableObject* _obj) override {}
        void CreateStaticSamplers();
        void CreateClearPipelines();
#ifdef __OBJC__
        id<MTLTexture> CreateTextureView(id<MTLTexture> base_texture, GfxTexture const* gfx_texture, GfxTextureDescriptorDesc const* desc);
#endif
    };
}
