#pragma once
#include "Graphics/GfxProfiler.h"
#include "Graphics/GfxTimestampProfilerFwd.h"

namespace adria
{
    using GfxProfilerTreeNode = typename GfxProfilerTree::NodeType;

    class MetalTimestampProfiler final : public GfxProfiler
    {
    public:
        MetalTimestampProfiler();
        ~MetalTimestampProfiler();

        virtual void Initialize(GfxDevice* gfx) override;
        virtual void Shutdown() override;
        virtual void NewFrame() override;
        virtual void BeginProfileScope(GfxCommandList* cmd_list, const char* name, bool active = true) override;
        virtual void EndProfileScope(GfxCommandList* cmd_list) override;
        virtual GfxProfilerTree const* GetProfilerTree() const override;

    private:
        GfxDevice* gfx = nullptr;
        Bool valid = false;
        void* counter_sample_buffer_handle = nullptr;

        static constexpr Uint64 FRAME_COUNT = GFX_BACKBUFFER_COUNT;
        static constexpr Uint64 MAX_PROFILES = 256;
        static constexpr Uint64 MAX_ENCODERS = 512;
        static constexpr Uint64 MAX_SAMPLE_COUNT = MAX_ENCODERS * 2 * FRAME_COUNT;

        GfxProfilerTreeAllocator profile_allocators[FRAME_COUNT];
        GfxProfilerTree profiler_trees[FRAME_COUNT];
        GfxProfilerTree* current_profiler_tree = nullptr;

        struct QueryData
        {
            GfxCommandList* cmd_list = nullptr;
            GfxProfilerTreeNode* tree_node = nullptr;
        };
        std::stack<QueryData> query_data;
        Uint32 scope_counter = 0;

        struct EncoderTimestamp
        {
            Uint32 scope_index;
            Uint32 begin_sample;
            Uint32 end_sample;
        };
        std::vector<EncoderTimestamp> encoder_timestamps;
        Uint32 encoder_counter = 0;
        Uint64 frame_sample_offset = 0;

        ProfilerMutexT profiler_mutex;
    private:
        Uint32 AllocateEncoderTimestamp();
    };
}
