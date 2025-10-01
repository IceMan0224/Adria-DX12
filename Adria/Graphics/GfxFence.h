#pragma once

namespace adria
{
	class GfxDevice;
	class GfxCommandQueue;

	class GfxFence
	{
	public:
		GfxFence() {}
		virtual ~GfxFence() {}

		virtual Bool Create(GfxDevice* gfx, Char const* name) = 0;
		virtual void Wait(Uint64 value) = 0;
		virtual void Signal(Uint64 value) = 0;
		virtual Bool IsCompleted(Uint64 value) = 0;
		virtual Uint64 GetCompletedValue() const = 0;
		virtual void* GetHandle() const = 0;
	};

	class GfxFence
	{
	public:
		GfxFence();
		~GfxFence();

		Bool Create(GfxDevice* gfx, Char const* name);

		void Wait(Uint64 value);
		void Signal(Uint64 value);

		Bool IsCompleted(Uint64 value);
		Uint64 GetCompletedValue() const;

		operator ID3D12Fence* () const { return fence.Get(); }

	};
}