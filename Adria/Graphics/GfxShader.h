#pragma once

namespace adria
{
	enum class GfxShaderStage
	{
		VS,
		PS,
		HS,
		DS,
		GS,
		CS,
		LIB,
		MS,
		AS,
		ShaderStageCount
	};
	enum GfxShaderModel
	{
		SM_Unknown,
		SM_6_0,
		SM_6_1,
		SM_6_2,
		SM_6_3,
		SM_6_4,
		SM_6_5,
		SM_6_6,
		SM_6_7,
		SM_6_8
	};

	struct GfxShaderDefine
	{
		std::string name;
		std::string value;
	};
	
	enum GfxShaderCompilerFlagBit
	{
		GfxShaderCompilerFlag_None = 0,
		GfxShaderCompilerFlag_Debug = 1 << 0,
		GfxShaderCompilerFlag_DisableOptimization = 1 << 1
	};
	using GfxShaderCompilerFlags = Uint32;
	struct GfxShaderDesc
	{
		GfxShaderStage stage = GfxShaderStage::ShaderStageCount;
		GfxShaderModel model = SM_6_7;
		std::string file = "";
		std::string entry_point = "";
		std::vector<GfxShaderDefine> defines{};
		GfxShaderCompilerFlags flags = GfxShaderCompilerFlag_None;
	};

	using GfxShaderBlob = std::vector<Uint8>;
	class GfxShader
	{
	public:
		void SetDesc(GfxShaderDesc const& _desc)
		{
			desc = _desc;
		}
		void SetShaderData(void const* data, Uint64 size)
		{
			shader_blob.resize(size);
			memcpy(shader_blob.data(), data, size);
		}

#if defined(ADRIA_PLATFORM_MACOS)
		void SetReflectionData(void const* reflection, Uint64 size)
		{
			if (reflection && size > 0)
			{
				reflection_data.resize(size);
				memcpy(reflection_data.data(), reflection, size);
			}
		}

		void const* GetReflectionData() const
		{
			return !reflection_data.empty() ? reflection_data.data() : nullptr;
		}

		Uint64 GetReflectionSize() const
		{
			return reflection_data.size();
		}
#endif

		GfxShaderDesc const& GetDesc() const { return desc; }

		void* GetData() const
		{
			return !shader_blob.empty() ? (void*)shader_blob.data() : nullptr;
		}
		Uint64 GetSize() const
		{
			return shader_blob.size();
		}

	private:
		GfxShaderBlob shader_blob;
		GfxShaderDesc desc;

#if defined(ADRIA_PLATFORM_MACOS)
		std::vector<Uint8> reflection_data;
#endif
	};
}