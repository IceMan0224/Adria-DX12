#import <Metal/Metal.h>
#include "MetalTimestampProfiler.h"
#include "MetalCommandList.h"
#include "MetalDevice.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"

namespace adria
{
    MetalTimestampProfiler::MetalTimestampProfiler()
        : profiler_trees{ GfxProfilerTree(profile_allocators[0]), GfxProfilerTree(profile_allocators[1]), GfxProfilerTree(profile_allocators[2]) }
    {
    }

    MetalTimestampProfiler::~MetalTimestampProfiler()
    {
        if (counter_sample_buffer_handle)
        {
            id<MTLCounterSampleBuffer> csb = (__bridge_transfer id<MTLCounterSampleBuffer>)counter_sample_buffer_handle;
            csb = nil;
            counter_sample_buffer_handle = nullptr;
        }
    }

    void MetalTimestampProfiler::Initialize(GfxDevice* _gfx)
    {
#if GFX_PROFILING
        if (_gfx->GetBackend() != GfxBackend::Metal)
        {
            ADRIA_ASSERT_MSG(false, "MetalTimestampProfiler only supports Metal backend!");
            return;
        }
        gfx = _gfx;

        MetalDevice* metal_device = static_cast<MetalDevice*>(gfx);
        id<MTLDevice> device = metal_device->GetMTLDevice();

        id<MTLCounterSet> timestamp_counter_set = nil;
        for (id<MTLCounterSet> cs in device.counterSets)
        {
            if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp])
            {
                timestamp_counter_set = cs;
                break;
            }
        }

        if (!timestamp_counter_set)
        {
            valid = false;
            return;
        }

        MTLCounterSampleBufferDescriptor* csbd = [[MTLCounterSampleBufferDescriptor alloc] init];
        csbd.counterSet = timestamp_counter_set;
        csbd.sampleCount = MAX_SAMPLE_COUNT;
        csbd.storageMode = MTLStorageModeShared;

        NSError* error = nil;
        id<MTLCounterSampleBuffer> csb = [device newCounterSampleBufferWithDescriptor:csbd error:&error];
        if (error || !csb)
        {
            valid = false;
            return;
        }
        counter_sample_buffer_handle = (__bridge_retained void*)csb;
        valid = true;
#endif
    }

    void MetalTimestampProfiler::Shutdown()
    {
#if GFX_PROFILING
        if (counter_sample_buffer_handle)
        {
            id<MTLCounterSampleBuffer> csb = (__bridge_transfer id<MTLCounterSampleBuffer>)counter_sample_buffer_handle;
            csb = nil;
            counter_sample_buffer_handle = nullptr;
        }
        gfx = nullptr;
        valid = false;
#endif
    }

    void MetalTimestampProfiler::NewFrame()
    {
#if GFX_PROFILING
        if (!valid) 
        {
            return;
        }

        ADRIA_ASSERT(query_data.empty());
        current_profiler_tree = &profiler_trees[gfx->GetBackbufferIndex()];
        current_profiler_tree->Clear();
        profile_allocators[gfx->GetBackbufferIndex()].Reset();
        scope_counter = 0;
        encoder_counter = 0;
        encoder_timestamps.clear();
        frame_sample_offset = gfx->GetBackbufferIndex() * MAX_ENCODERS * 2;
#endif
    }

    Uint32 MetalTimestampProfiler::AllocateEncoderTimestamp()
    {
        Uint32 encoder_index = encoder_counter++;
        if (encoder_index >= MAX_ENCODERS) 
        {
            return UINT32_MAX;
        }
        return encoder_index;
    }

    void MetalTimestampProfiler::BeginProfileScope(GfxCommandList* cmd_list, const char* name, bool active)
    {
#if GFX_PROFILING
        if (!valid || !active) 
        {
            return;
        }
#if GFX_MULTITHREADED
        std::lock_guard lock(profiler_mutex);
#endif
        Uint32 profile_index = scope_counter++;
        if (profile_index >= MAX_PROFILES) 
        {
            return;
        }

        GfxProfilerTreeNode* tree_node = nullptr;
        if (!query_data.empty())
        {
            QueryData& parent_data = query_data.top();
            tree_node = parent_data.tree_node->EmplaceChild(name, cmd_list, profile_index, 0.0f);
        }
        else
        {
            ADRIA_ASSERT(current_profiler_tree->GetRoot() == nullptr);
            current_profiler_tree->EmplaceRoot(name, cmd_list, profile_index, 0.0f);
            tree_node = current_profiler_tree->GetRoot();
        }
        query_data.emplace(cmd_list, tree_node);

        Uint32 encoder_index = AllocateEncoderTimestamp();
        if (encoder_index == UINT32_MAX) return;

        Uint32 begin_sample = static_cast<Uint32>(frame_sample_offset) + encoder_index * 2;
        Uint32 end_sample = begin_sample + 1;
        encoder_timestamps.push_back({ profile_index, begin_sample, end_sample });
        MetalCommandList* metal_cmd_list = static_cast<MetalCommandList*>(cmd_list);
        metal_cmd_list->SetPendingTimestampSample(counter_sample_buffer_handle, begin_sample, end_sample);
#endif
    }

    void MetalTimestampProfiler::EndProfileScope(GfxCommandList* cmd_list)
    {
#if GFX_PROFILING
        if (!valid) 
        {
            return;
        }
        ADRIA_ASSERT(!query_data.empty());
#if GFX_MULTITHREADED
        std::lock_guard lock(profiler_mutex);
#endif
        query_data.pop();
#endif
    }

    GfxProfilerTree const* MetalTimestampProfiler::GetProfilerTree() const
    {
#if GFX_PROFILING
        if (!valid || !current_profiler_tree || !current_profiler_tree->GetRoot()) 
        {
            return nullptr;
        }
        if (encoder_counter == 0) 
        {
            return nullptr;
        }

        Uint64 gpu_frequency = 0;
        gfx->GetTimestampFrequency(gpu_frequency);

        id<MTLCounterSampleBuffer> csb = (__bridge id<MTLCounterSampleBuffer>)counter_sample_buffer_handle;
        NSRange resolve_range = NSMakeRange(frame_sample_offset, encoder_counter * 2);
        NSData* resolved_data = [csb resolveCounterRange:resolve_range];
        if (!resolved_data) return nullptr;

        MTLCounterResultTimestamp const* all_timestamps = static_cast<MTLCounterResultTimestamp const*>([resolved_data bytes]);

        struct ScopeTime
        {
            Uint64 min_begin = UINT64_MAX;
            Uint64 max_end = 0;
            Bool has_data = false;
        };
        std::vector<ScopeTime> scope_times(scope_counter);
        for (auto const& et : encoder_timestamps)
        {
            if (et.scope_index >= scope_counter)
            {
                 continue;
            }

            Uint32 local_begin = et.begin_sample - static_cast<Uint32>(frame_sample_offset);
            Uint32 local_end = et.end_sample - static_cast<Uint32>(frame_sample_offset);

            Uint64 begin_ts = all_timestamps[local_begin].timestamp;
            Uint64 end_ts = all_timestamps[local_end].timestamp;

            if (begin_ts == 0 && end_ts == 0)
            {
                 continue;
            }

            auto& st = scope_times[et.scope_index];
            st.min_begin = std::min(st.min_begin, begin_ts);
            st.max_end = std::max(st.max_end, end_ts);
            st.has_data = true;
        }

        Float frequency = Float(gpu_frequency);
        current_profiler_tree->TraversePostOrder([&scope_times, frequency](GfxProfilerTreeNode* node)
            {
                auto const& children = node->GetChildren();
                if (!children.empty())
                {
                    Float64 sum = 0.0;
                    for (auto const& child : children)
                    {
                        sum += child->GetData().time;
                    }
                    node->GetData().time = sum;
                    return;
                }
                Uint32 const index = node->GetData().index;
                if (index >= scope_times.size())
                {
                    node->GetData().time = 0.0f;
                    return;
                }
                auto const& st = scope_times[index];
                if (st.has_data && st.max_end > st.min_begin)
                {
                    Uint64 delta = st.max_end - st.min_begin;
                    node->GetData().time = (delta / frequency) * 1000.0f;
                }
                else
                {
                    node->GetData().time = 0.0f;
                }
            });
        return current_profiler_tree;
#else
        return nullptr;
#endif
    }
}
