#include "GpuPrintf.h"
#if GFX_SHADER_PRINTF
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "RenderGraph/RenderGraph.h"
#include "Utilities/BufferReader.h"
#endif

namespace adria
{
#if GFX_SHADER_PRINTF

	enum ArgCode
	{
		DebugPrint_Uint = 0,
		DebugPrint_Uint2,
		DebugPrint_Uint3,
		DebugPrint_Uint4,
		DebugPrint_Int,
		DebugPrint_Int2,
		DebugPrint_Int3,
		DebugPrint_Int4,
		DebugPrint_Float,
		DebugPrint_Float2,
		DebugPrint_Float3,
		DebugPrint_Float4,
		NumDebugPrintArgCodes
	};
	constexpr Uint32 ArgCodeSizes[NumDebugPrintArgCodes] =
	{
		4, 8, 12, 16,
		4, 8, 12, 16,
		4, 8, 12, 16
	};
	struct DebugPrintHeader
	{
		Uint32 NumBytes;
		Uint32 StringSize;
		Uint32 NumArgs;
	};
	
	static std::string MakeArgString(BufferReader& reader, ArgCode arg_code)
	{
		switch (arg_code)
		{
		case DebugPrint_Uint:
		{
			Uint32 value = *reader.Consume<Uint32>();
			return std::to_string(value);
		}
		case DebugPrint_Uint2:
		{
			struct Uint2
			{
				Uint32 value1;
				Uint32 value2;
			};
			Uint2 uint2 = *reader.Consume<Uint2>();
			return "(" + std::to_string(uint2.value1) + "," + std::to_string(uint2.value2) + ")";
		}
		case DebugPrint_Uint3:
		{
			struct Uint3
			{
				Uint32 value1;
				Uint32 value2;
				Uint32 value3;
			};
			Uint3 uint3 = *reader.Consume<Uint3>();
			return "(" + std::to_string(uint3.value1) + "," + std::to_string(uint3.value2) + "," + std::to_string(uint3.value3) + ")";
		}
		case DebugPrint_Uint4:
		{
			struct Uint4
			{
				Uint32 value1;
				Uint32 value2;
				Uint32 value3;
				Uint32 value4;
			};
			Uint4 uint4 = *reader.Consume<Uint4>();
			return "(" + std::to_string(uint4.value1) + "," + std::to_string(uint4.value2) + "," + std::to_string(uint4.value3) + "," + std::to_string(uint4.value4) + ")";
		}
		case DebugPrint_Int:
		{
			Int32 value = *reader.Consume<Int32>();
			return std::to_string(value);
		}
		case DebugPrint_Int2:
		{
			struct Int2
			{
				Int32 value1;
				Int32 value2;
			};
			Int2 int2 = *reader.Consume<Int2>();
			return "(" + std::to_string(int2.value1) + "," + std::to_string(int2.value2) + ")";
		}
		case DebugPrint_Int3:
		{
			struct Int3
			{
				Int32 value1;
				Int32 value2;
				Int32 value3;
			};
			Int3 int3 = *reader.Consume<Int3>();
			return "(" + std::to_string(int3.value1) + "," + std::to_string(int3.value2) + "," + std::to_string(int3.value3) + ")";
		}
		case DebugPrint_Int4:
		{
			struct Int4
			{
				Int32 value1;
				Int32 value2;
				Int32 value3;
				Int32 value4;
			};
			Int4 int4 = *reader.Consume<Int4>();
			return "(" + std::to_string(int4.value1) + "," + std::to_string(int4.value2) + "," + std::to_string(int4.value3) + "," + std::to_string(int4.value4) + ")";
		}
		case DebugPrint_Float:
		{
			Float value = *reader.Consume<Float>();
			return std::to_string(value);
		}
		case DebugPrint_Float2:
		{
			struct Float2
			{
				Float value1;
				Float value2;
			};
			Float2  float2 = *reader.Consume<Float2>();
			return "(" + std::to_string(float2.value1) + "," + std::to_string(float2.value2) + ")";
		}
		case DebugPrint_Float3:
		{
			struct Float3
			{
				Float value1;
				Float value2;
				Float value3;
			};
			Float3 float3 = *reader.Consume<Float3>();
			return "(" + std::to_string(float3.value1) + "," + std::to_string(float3.value2) + "," + std::to_string(float3.value3) + ")";
		}
		case DebugPrint_Float4:
		{
			struct Float4
			{
				Float value1;
				Float value2;
				Float value3;
				Float value4;
			};
			Float4 float4 = *reader.Consume<Float4>();
			return "(" + std::to_string(float4.value1) + "," + std::to_string(float4.value2) + "," + std::to_string(float4.value3) + "," + std::to_string(float4.value4) + ")";
		}
		case NumDebugPrintArgCodes:
		default:
			ADRIA_ASSERT(false);
		}
		return "";
	}

	GpuPrintf::GpuPrintf(GfxDevice* gfx) : GpuDebugFeature(gfx, RG_NAME(GpuPrintfBuffer)) {}

	Int32 GpuPrintf::GetPrintfBufferIndex()
	{
		return GetBufferIndex();
	}

	void GpuPrintf::AddClearPass(RenderGraph& rg)
	{
		return GpuDebugFeature::AddClearPass(rg, "Clear Printf Buffer Pass");
	}

	void GpuPrintf::AddPrintPass(RenderGraph& rg)
	{
		return GpuDebugFeature::AddFeaturePass(rg, "Copy Printf Buffer Pass");
	}

	void GpuPrintf::ProcessBufferData(GfxBuffer& old_readback_buffer)
	{
		static constexpr Uint32 MaxDebugPrintArgs = 4;
		BufferReader printf_reader(old_readback_buffer.GetMappedData<Uint8>() + sizeof(Uint32), (Uint32)old_readback_buffer.GetSize() - sizeof(Uint32));
		while (printf_reader.HasMoreData(sizeof(DebugPrintHeader)))
		{
			DebugPrintHeader const* header = printf_reader.Consume<DebugPrintHeader>();
			if (header->NumBytes == 0 || header->NumArgs > MaxDebugPrintArgs || !printf_reader.HasMoreData(header->NumBytes))
			{
				break;
			}

			std::string fmt = printf_reader.ConsumeString(header->StringSize);
			if (fmt.length() == 0)
			{
				break;
			}

			std::vector<std::string> arg_strings;
			arg_strings.reserve(header->NumArgs);
			for (Uint32 arg_idx = 0; arg_idx < header->NumArgs; ++arg_idx)
			{
				ArgCode const arg_code = (ArgCode)*printf_reader.Consume<Uint8>();
				if (arg_code >= NumDebugPrintArgCodes || arg_code < 0)
				{
					break;
				}

				Uint32 const arg_size = ArgCodeSizes[arg_code];
				if (!printf_reader.HasMoreData(arg_size))
				{
					break;
				}

				std::string const arg_string = MakeArgString(printf_reader, arg_code);
				arg_strings.push_back(arg_string);
			}

			if (header->NumArgs > 0)
			{
				for (Uint64 i = 0; i < arg_strings.size(); ++i)
				{
					std::string placeholder = "{" + std::to_string(i) + "}";
					Uint64 pos = fmt.find(placeholder);
					while (pos != std::string::npos)
					{
						fmt.replace(pos, placeholder.length(), arg_strings[i]);
						pos = fmt.find(placeholder, pos + arg_strings[i].length());
					}
				}
			}
			ADRIA_LOG(INFO, fmt.c_str());
		}
	}

#else
	GpuPrintf::GpuPrintf(GfxDevice* gfx) : GpuDebugFeature(gfx, RG_NAME(PrintfBuffer)) {}
	Int32 GpuPrintf::GetPrintfBufferIndex() { return -1; }
	void GpuPrintf::AddClearPass(RenderGraph& rg) {}
	void GpuPrintf::AddPrintPass(RenderGraph& rg) {}
	void GpuPrintf::ProcessBufferData(GfxBuffer&) {}
#endif
	GpuPrintf::~GpuPrintf() = default;
}

