#pragma once
#include "GfxFormat.h"
#include "Utilities/Enum.h"

namespace adria
{
	struct GfxClearValue
	{
		enum class GfxActiveMember
		{
			None,
			Color,
			DepthStencil
		};
		struct GfxClearColor
		{
			GfxClearColor(Float r = 0.0f, Float g = 0.0f, Float b = 0.0f, Float a = 0.0f)
				: color{ r, g, b, a }
			{
			}
			GfxClearColor(Float(&_color)[4])
				: color{ _color[0], _color[1], _color[2], _color[3] }
			{
			}
			GfxClearColor(GfxClearColor const& other)
				: color{ other.color[0], other.color[1], other.color[2], other.color[3] }
			{
			}

			Bool operator==(GfxClearColor const& other) const
			{
				return memcmp(color, other.color, sizeof(color)) == 0;
			}

			Float color[4];
		};
		struct GfxClearDepthStencil
		{
			GfxClearDepthStencil(Float depth = 0.0f, Uint8 stencil = 1)
				: depth(depth), stencil(stencil)
			{}
			Float depth;
			Uint8 stencil;
		};

		GfxClearValue() : active_member(GfxActiveMember::None), depth_stencil{} {}

		GfxClearValue(Float r, Float g, Float b, Float a)
			: active_member(GfxActiveMember::Color), color(r, g, b, a)
		{
		}

		GfxClearValue(Float(&_color)[4])
			: active_member(GfxActiveMember::Color), color{ _color }
		{
		}

		GfxClearValue(GfxClearColor const& color)
			: active_member(GfxActiveMember::Color), color(color)
		{}

		GfxClearValue(Float depth, Uint8 stencil)
			: active_member(GfxActiveMember::DepthStencil), depth_stencil(depth, stencil)
		{}
		GfxClearValue(GfxClearDepthStencil const& depth_stencil)
			: active_member(GfxActiveMember::DepthStencil), depth_stencil(depth_stencil)
		{}

		GfxClearValue(GfxClearValue const& other)
			: active_member(other.active_member), color{}, format(other.format)
		{
			if (active_member == GfxActiveMember::Color) color = other.color;
			else if (active_member == GfxActiveMember::DepthStencil) depth_stencil = other.depth_stencil;
		}

		GfxClearValue& operator=(GfxClearValue const& other)
		{
			if (this == &other) return *this;
			active_member = other.active_member;
			format = other.format;
			if (active_member == GfxActiveMember::Color) color = other.color;
			else if (active_member == GfxActiveMember::DepthStencil) depth_stencil = other.depth_stencil;
			return *this;
		}

		Bool operator==(GfxClearValue const& other) const
		{
			if (active_member != other.active_member) return false;
			else if (active_member == GfxActiveMember::Color)
			{
				return color == other.color;
			}
			else return depth_stencil.depth == other.depth_stencil.depth
				&& depth_stencil.stencil == other.depth_stencil.stencil;
		}

		GfxActiveMember active_member;
		GfxFormat format = GfxFormat::UNKNOWN;
		union
		{
			GfxClearColor color;
			GfxClearDepthStencil depth_stencil;
		};
	};

	enum class GfxSubresourceType : Uint8
	{
		SRV,
		UAV,
		RTV,
		DSV,
		Invalid
	};

	enum class GfxBindFlag : Uint32
	{
		None = 0,
		ShaderResource = BIT(0),
		RenderTarget = BIT(1),
		DepthStencil = BIT(2),
		UnorderedAccess = BIT(3),
	};
	ENABLE_ENUM_BIT_OPERATORS(GfxBindFlag);

	enum class GfxResourceUsage : Uint8
	{
		Default,
		Upload,
		Readback
	};

	enum class GfxTextureMiscFlag : Uint32
	{
		None = 0,
		TextureCube = BIT(0),
		SRGB = BIT(1),
		Shared = BIT(2)
	};
	ENABLE_ENUM_BIT_OPERATORS(GfxTextureMiscFlag);

	enum class GfxBufferMiscFlag : Uint32
	{
		None = 0,
		IndirectArgs = BIT(0),
		BufferRaw = BIT(1),
		BufferStructured = BIT(2),
		ConstantBuffer = BIT(3),
		VertexBuffer = BIT(4),
		IndexBuffer = BIT(5),
		AccelStruct = BIT(6),
		Shared = BIT(7)
	};
	ENABLE_ENUM_BIT_OPERATORS(GfxBufferMiscFlag);

	enum class GfxResourceState : Uint64
	{
		None = 0,
		Common = BIT(0),
		Present = BIT(1),
		RTV = BIT(2),
		DSV = BIT(3),
		DSV_ReadOnly = BIT(4),
		VertexSRV = BIT(5),
		PixelSRV = BIT(6),
		ComputeSRV = BIT(7),
		VertexUAV = BIT(8),
		PixelUAV = BIT(9),
		ComputeUAV = BIT(10),
		ClearUAV = BIT(11),
		CopyDst = BIT(12),
		CopySrc = BIT(13),
		ShadingRate = BIT(14),
		IndexBuffer = BIT(15),
		IndirectArgs = BIT(16),
		ASRead = BIT(17),
		ASWrite = BIT(18),
		Discard = BIT(19),

		AllVertex = VertexSRV | VertexUAV,
		AllPixel = PixelSRV | PixelUAV,
		AllCompute = ComputeSRV | ComputeUAV,
		AllSRV = VertexSRV | PixelSRV | ComputeSRV,
		AllUAV = VertexUAV | PixelUAV | ComputeUAV,
		AllDSV = DSV | DSV_ReadOnly,
		AllCopy = CopyDst | CopySrc,
		AllAS = ASRead | ASWrite,
		GenericRead = CopySrc | AllSRV,
		GenericWrite = CopyDst | AllUAV,
		AllShading = AllSRV | AllUAV | ShadingRate | ASRead
	};
	ENABLE_ENUM_BIT_OPERATORS(GfxResourceState);

	inline constexpr std::string ConvertBarrierFlagsToString(GfxResourceState flags)
	{
		ADRIA_TODO("Finish this");
		std::string resource_state_string = "";
		if (!resource_state_string.empty())
		{
			resource_state_string.pop_back();
		}
		return resource_state_string.empty() ? "Common" : resource_state_string;
	}
}
