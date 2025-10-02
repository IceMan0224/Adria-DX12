#pragma once
#include "GfxResource.h"

namespace adria
{
	enum GfxTextureType : Uint8
	{
		GfxTextureType_1D,
		GfxTextureType_2D,
		GfxTextureType_3D
	};
	inline Char const* GfxTextureTypeToString(GfxTextureType type)
	{
		switch (type)
		{
		case GfxTextureType_1D: return "Texture 1D";
		case GfxTextureType_2D: return "Texture 2D";
		case GfxTextureType_3D: return "Texture 3D";
		default: return "Invalid";
		}
	}

	struct GfxTextureDesc
	{
		GfxTextureType type = GfxTextureType_2D;
		Uint32 width = 0;
		Uint32 height = 0;
		Uint32 depth = 0;
		Uint32 array_size = 1;
		Uint32 mip_levels = 1;
		Uint32 sample_count = 1;
		GfxResourceUsage heap_type = GfxResourceUsage::Default;
		GfxBindFlag bind_flags = GfxBindFlag::None;
		GfxTextureMiscFlag misc_flags = GfxTextureMiscFlag::None;
		GfxClearValue clear_value{};
		GfxResourceState initial_state = GfxResourceState::AllSRV;
		GfxFormat format = GfxFormat::UNKNOWN;

		std::strong_ordering operator<=>(GfxTextureDesc const& other) const = default;
		Bool IsCompatible(GfxTextureDesc const& desc) const
		{
			return type == desc.type && width == desc.width && height == desc.height && array_size == desc.array_size
				&& format == desc.format && sample_count == desc.sample_count && heap_type == desc.heap_type
				&& HasAllFlags(bind_flags, desc.bind_flags) && HasAllFlags(misc_flags, desc.misc_flags) && clear_value == desc.clear_value;
		}
	};

	enum GfxTextureDescriptorFlags : Uint8
	{
		GfxTextureDescriptorFlag_None = 0,
		GfxTextureDescriptorFlag_DepthReadOnly = BIT(0)
	};

	enum GfxTextureChannelMapping : Uint32
	{
		GfxTextureChannelMapping_Red,
		GfxTextureChannelMapping_Green,
		GfxTextureChannelMapping_Blue,
		GfxTextureChannelMapping_Alpha,
		GfxTextureChannelMapping_Zero,
		GfxTextureChannelMapping_One,
	};

	inline constexpr GfxTextureChannelMapping GfxCustomTextureChannelMapping(GfxTextureChannelMapping R, GfxTextureChannelMapping G,
		GfxTextureChannelMapping B, GfxTextureChannelMapping A)
	{
		constexpr Uint32 GfxTextureChannelMappingMask = 0x7;
		constexpr Uint32 GfxTextureChannelMappingShift = 3;
		constexpr Uint32 GfxTextureChannelMappingAlwaysSetBit = 1 << (GfxTextureChannelMappingShift * 4);

		return GfxTextureChannelMapping(
			(((R) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 0)) |
			(((G) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 1)) |
			(((B) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 2)) |
			(((A) & GfxTextureChannelMappingMask) << (GfxTextureChannelMappingShift * 3)) |
			GfxTextureChannelMappingAlwaysSetBit);
	}

	inline constexpr GfxTextureChannelMapping GfxDefaultTextureChannelMapping = GfxCustomTextureChannelMapping(GfxTextureChannelMapping_Red, GfxTextureChannelMapping_Green,
																											   GfxTextureChannelMapping_Blue, GfxTextureChannelMapping_Alpha);
	inline constexpr GfxTextureChannelMapping GfxAlphaOneTextureChannelMapping = GfxCustomTextureChannelMapping(GfxTextureChannelMapping_Red, GfxTextureChannelMapping_Green,
																											    GfxTextureChannelMapping_Blue, GfxTextureChannelMapping_One);

	struct GfxTextureDescriptorDesc
	{
		Uint32 first_slice = 0;
		Uint32 slice_count = static_cast<Uint32>(-1);
		Uint32 first_mip = 0;
		Uint32 mip_count = static_cast<Uint32>(-1);

		GfxTextureDescriptorFlags flags = GfxTextureDescriptorFlag_None;
		GfxTextureChannelMapping channel_mapping = GfxDefaultTextureChannelMapping;
		std::strong_ordering operator<=>(GfxTextureDescriptorDesc const& other) const = default;
	};

	struct GfxTextureSubData
	{
		void const* data;
		Uint64 row_pitch;
		Uint64 slice_pitch;
	};

	struct GfxTextureData
	{
		GfxTextureSubData* sub_data = nullptr;
		Uint32 sub_count = Uint32(-1);
	};
	
	class GfxDevice;
	class GfxTexture
	{
	public:
		ADRIA_NONCOPYABLE_NONMOVABLE(GfxTexture)
		virtual ~GfxTexture() {};

		virtual void* GetNative() const = 0;
		virtual void* GetSharedHandle() const = 0;
		virtual Uint64 GetGpuAddress() const = 0;
		virtual void* Map() = 0;
		virtual void Unmap() = 0;
		virtual void SetName(Char const* name) = 0;
		virtual Uint32 GetRowPitch(Uint32 mip_level = 0) const = 0;

		ADRIA_FORCEINLINE GfxDevice* GetParent() const { return gfx; }
		ADRIA_FORCEINLINE GfxTextureDesc const& GetDesc() const { return desc; }
		ADRIA_FORCEINLINE Uint32 GetWidth() const { return desc.width; }
		ADRIA_FORCEINLINE Uint32 GetHeight() const { return desc.height; }
		ADRIA_FORCEINLINE Uint32 GetDepth() const { return desc.depth; }
		ADRIA_FORCEINLINE GfxFormat GetFormat() const { return desc.format; }
		ADRIA_FORCEINLINE Bool IsSRGB() const { return (desc.misc_flags & GfxTextureMiscFlag::SRGB) != GfxTextureMiscFlag::None; }

		Bool IsMapped() const { return mapped_data != nullptr; }
		void* GetMappedData() const { return mapped_data; }
		template<typename T>
		T* GetMappedData() const
		{
			return reinterpret_cast<T*>(mapped_data);
		}

	protected:
		GfxDevice* gfx;
		GfxTextureDesc desc;
		void* mapped_data = nullptr;
		Bool is_backbuffer = false;

	protected:
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc) : gfx(gfx), desc(desc) {}
		GfxTexture(GfxDevice* gfx, GfxTextureDesc const& desc, void* backbuffer) : gfx(gfx), desc(desc), is_backbuffer(true) {}
	};
}