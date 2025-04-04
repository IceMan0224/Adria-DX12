#include "GpuAssert.h"
#if GFX_SHADER_ASSERT
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "RenderGraph/RenderGraph.h"
#include "Core/FatalAssert.h"
#endif

ADRIA_DEBUGZONE_BEGIN

namespace adria
{
#if GFX_SHADER_ASSERT
	enum GpuAssertType : Uint32
	{
		GpuAssertType_Invalid,
		GpuAssertType_Generic,
		GpuAssertType_IndexOutOfBounds
	};

	struct GpuAssertHeader
	{
		Uint32 type;
		Uint32 arg_count;
	};

	struct GpuAssertArgs1
	{
		Uint32 arg0;
	};
	struct GpuAssertArgs2
	{
		Uint32 arg0, arg1;
	};
	struct GpuAssertArgs3
	{
		Uint32 arg0, arg1, arg2;
	};
	struct GpuAssertArgs4
	{
		Uint32 arg0, arg1, arg2, arg3;
	};

	struct AssertBufferReader
	{
		AssertBufferReader(Uint8* data, Uint32 size) : data(data), size(size), current_offset(0) {}

		Bool HasMoreData(Uint32 count) const
		{
			return current_offset + count <= size;
		}
		template<typename T>
		T* Consume()
		{
			T* consumed_data = reinterpret_cast<T*>(data + current_offset);
			current_offset += sizeof(T);
			return consumed_data;
		}
		std::string ConsumeString(Uint32 char_count)
		{
			Char* char_data = (Char*)data;
			std::string consumed_string(char_data + current_offset, char_count);
			current_offset += char_count;
			return consumed_string;
		}

		Uint8* data;
		Uint32 const size;
		Uint32 current_offset;
	};

	std::string GetAssertArgs(AssertBufferReader& reader, Uint32 arg_count)
	{
		switch (arg_count)
		{
		case 0: return "";
		case 1:
		{
			GpuAssertArgs1 args = *reader.Consume<GpuAssertArgs1>();
			return std::to_string(args.arg0);
		}
		case 2:
		{
			GpuAssertArgs2 args = *reader.Consume<GpuAssertArgs2>();
			return "(" + std::to_string(args.arg0) + "," + std::to_string(args.arg1) + ")";
		}
		case 3:
		{
			GpuAssertArgs3 args = *reader.Consume<GpuAssertArgs3>();
			return "(" + std::to_string(args.arg0) + "," + std::to_string(args.arg1) + "," + std::to_string(args.arg2) + ")";
		}
		case 4:
		{
			GpuAssertArgs4 args = *reader.Consume<GpuAssertArgs4>();
			return "(" + std::to_string(args.arg0) + "," + std::to_string(args.arg1) + "," + std::to_string(args.arg2) + "," + std::to_string(args.arg3) + ")";
		}
		}
		ADRIA_UNREACHABLE();
		return "";
	}
	static std::string GetAssertMessage(AssertBufferReader& reader, Uint32 arg_count, GpuAssertType type)
	{
		switch (type)
		{
		case GpuAssertType_Generic:
		{
			return "Generic Gpu Assert:" + GetAssertArgs(reader, arg_count);
		}
		case GpuAssertType_IndexOutOfBounds:
			ADRIA_ASSERT(arg_count == 2);
			return "IndexOutOfBounds Gpu Assert:" + GetAssertArgs(reader, arg_count);
		}
		ADRIA_UNREACHABLE();
		return "";
	}

	GpuAssert::GpuAssert(GfxDevice* gfx) : GpuDebugFeature(gfx, RG_NAME(GpuAssertBuffer)) {}
	Int32 GpuAssert::GetAssertBufferIndex() { return GetBufferIndex(); }
	void GpuAssert::AddClearPass(RenderGraph& rg) { return GpuDebugFeature::AddClearPass(rg, "Clear Assert Buffer Pass"); }
	void GpuAssert::AddAssertPass(RenderGraph& rg) { return GpuDebugFeature::AddFeaturePass(rg, "Copy Assert Buffer Pass"); }
	void GpuAssert::ProcessBufferData(GfxBuffer& old_readback_buffer)
	{
		static constexpr Uint32 MaxGpuAssertArgs = 4;
		AssertBufferReader assert_reader(old_readback_buffer.GetMappedData<Uint8>() + sizeof(Uint32), (Uint32)old_readback_buffer.GetSize() - sizeof(Uint32));
		while (assert_reader.HasMoreData(sizeof(GpuAssertHeader)))
		{
			GpuAssertHeader const* header = assert_reader.Consume<GpuAssertHeader>();
			if (header->type == GpuAssertType_Invalid || header->arg_count > MaxGpuAssertArgs) break;

			std::string assert_msg = GetAssertMessage(assert_reader, header->arg_count, (GpuAssertType)header->type);
			ADRIA_FATAL_ASSERT(false, assert_msg.c_str());
		}
	}
#else
	GpuAssert::GpuAssert(GfxDevice* gfx) : GpuDebugFeature(gfx, RG_NAME(AssertBuffer)) {}
	Int32 GpuAssert::GetAssertBufferIndex() { return -1; }
	void GpuAssert::AddClearPass(RenderGraph& rg) {}
	void GpuAssert::AddAssertPass(RenderGraph& rg) {}
	void GpuAssert::ProcessBufferData(GfxBuffer&) {}
#endif
	GpuAssert::~GpuAssert() = default;
}

