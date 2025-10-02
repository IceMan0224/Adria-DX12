#pragma once

namespace adria
{
	class GfxDevice;
	class GfxCommandQueue;

	class GfxFence
	{
	public:
		virtual ~GfxFence() {}

		virtual Bool Create(GfxDevice* gfx, Char const* name) = 0;
		virtual void Wait(Uint64 value) = 0;
		virtual void Signal(Uint64 value) = 0;
		virtual Bool IsCompleted(Uint64 value) = 0;
		virtual Uint64 GetCompletedValue() const = 0;
		virtual void* GetHandle() const = 0;
	};
}