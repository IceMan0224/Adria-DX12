#pragma once
#include "Graphics/GfxFence.h"

#ifdef __OBJC__
    @protocol MTLSharedEvent;
    #define ID_POINTER(x) id<x>
#else
    #define ID_POINTER(x) void*
#endif

namespace adria
{
    class MetalFence final : public GfxFence
    {
    public:
        MetalFence();
        virtual ~MetalFence();

        virtual Bool Create(GfxDevice* gfx, Char const* name) override;
        virtual void Wait(Uint64 value) override;
        virtual void Signal(Uint64 value) override;
        virtual Bool IsCompleted(Uint64 value) override;
        virtual Uint64 GetCompletedValue() const override;
        virtual void* GetHandle() const override;

    private:
        ID_POINTER(MTLSharedEvent) shared_event = nullptr;
    };
}
