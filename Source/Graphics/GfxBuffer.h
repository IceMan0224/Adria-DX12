#pragma once
#include "GfxResource.h"

namespace adria
{
	struct GfxBufferDesc
	{
		Uint64 size = 0;
		GfxResourceUsage resource_usage = GfxResourceUsage::Default;
		GfxBindFlag bind_flags = GfxBindFlag::None;
		GfxBufferMiscFlag misc_flags = GfxBufferMiscFlag::None;
		Uint32 stride = 0; 
		GfxFormat format = GfxFormat::UNKNOWN; 
		Bool persistent = true;
		std::strong_ordering operator<=>(GfxBufferDesc const& other) const = default;
	};

	struct GfxBufferDescriptorDesc
	{
		Uint64 offset = 0;
		Uint64 size = Uint64(-1);
		std::strong_ordering operator<=>(GfxBufferDescriptorDesc const& other) const = default;
	};

	struct GfxBufferData
	{
		GfxBufferData() {}
		GfxBufferData(void const* _data) : data(_data) {}
		operator void const* () const { return data; }
		void const* data = nullptr;
	};

	class GfxDevice;
	class GfxBuffer
	{
	public:
		ADRIA_NONCOPYABLE_NONMOVABLE(GfxBuffer)
		virtual ~GfxBuffer() {}

		virtual void* GetNative() const = 0;
		virtual Uint64 GetGpuAddress() const = 0;
		virtual void* GetSharedHandle() const = 0;
		ADRIA_MAYBE_UNUSED virtual void* Map() = 0;
		virtual void Unmap() = 0;
		virtual void SetName(Char const* name) = 0;

		ADRIA_FORCEINLINE GfxDevice* GetParent() const { return gfx; }
		ADRIA_FORCEINLINE GfxBufferDesc const& GetDesc() const { return desc; }
		ADRIA_FORCEINLINE Uint64 GetSize() const { return desc.size; };
		ADRIA_FORCEINLINE Uint32 GetStride() const { return desc.stride; }
		ADRIA_FORCEINLINE Uint32 GetCount() const
		{
			ADRIA_ASSERT(desc.stride != 0);
			return static_cast<Uint32>(desc.size / desc.stride);
		}
		ADRIA_FORCEINLINE GfxFormat GetFormat() const { return desc.format; }

		ADRIA_FORCEINLINE Bool IsMapped() const { return mapped_data != nullptr; }
		ADRIA_FORCEINLINE void* GetMappedData() const { return mapped_data; }
		template<typename T>
		ADRIA_FORCEINLINE T* GetMappedData() const
		{
			return reinterpret_cast<T*>(mapped_data);
		}
		void Update(void const* src_data, Uint64 data_size, Uint64 offset = 0)
		{
			ADRIA_ASSERT(desc.resource_usage == GfxResourceUsage::Upload);
			if (mapped_data)
			{
				memcpy((Uint8*)mapped_data + offset, src_data, data_size);
			}
			else
			{
				Map();
				ADRIA_ASSERT(mapped_data);
				memcpy((Uint8*)mapped_data + offset, src_data, data_size);
			}
		}
		template<typename T>
		void Update(T const& src_data)
		{
			Update(&src_data, sizeof(T));
		}

		Bool IsPersistent() const { return desc.persistent; }
		void SetPersistent(Bool persistent) { desc.persistent = persistent; }

	protected:
		GfxDevice* gfx;
		GfxBufferDesc desc;
		void* mapped_data = nullptr;

	protected:
		GfxBuffer(GfxDevice* gfx, GfxBufferDesc const& desc) : gfx(gfx), desc(desc) {}
	};
}