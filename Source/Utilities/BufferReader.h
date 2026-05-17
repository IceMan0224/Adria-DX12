#pragma once

namespace adria
{
	struct BufferReader
	{
		BufferReader(Uint8* data, Uint32 size) : data(data), size(size), current_offset(0) {}

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
}