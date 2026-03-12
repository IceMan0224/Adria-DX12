#pragma once
#include "cereal/archives/binary.hpp"

namespace DirectX::SimpleMath
{
	template<typename Archive>
	void serialize(Archive& ar, Vector2& v) { ar(v.x, v.y); }

	template<typename Archive>
	void serialize(Archive& ar, Vector3& v) { ar(v.x, v.y, v.z); }

	template<typename Archive>
	void serialize(Archive& ar, Vector4& v) { ar(v.x, v.y, v.z, v.w); }

	template<typename Archive>
	void serialize(Archive& ar, Quaternion& q) { ar(q.x, q.y, q.z, q.w); }

	template<typename Archive>
	void serialize(Archive& ar, Color& c) { ar(c.x, c.y, c.z, c.w); }

	template<typename Archive>
	void serialize(Archive& ar, Matrix& m) { ar(cereal::binary_data(&m, sizeof(Matrix))); }
}

namespace adria
{
	class BinaryFileWriter
	{
	public:
		explicit BinaryFileWriter(std::string const& file_path)
			: stream(file_path, std::ios::binary)
		{
			if (stream.is_open())
				archive = std::make_unique<cereal::BinaryOutputArchive>(stream);
		}

		Bool IsValid() const { return archive != nullptr; }

		template<typename... Args>
		void Write(Args const&... args)
		{
			ADRIA_ASSERT(IsValid());
			(*archive)(args...);
		}

		void WriteBlob(void const* data, Usize size)
		{
			ADRIA_ASSERT(IsValid());
			archive->saveBinary(data, size);
		}

	private:
		std::ofstream stream;
		std::unique_ptr<cereal::BinaryOutputArchive> archive;
	};

	class BinaryFileReader
	{
	public:
		explicit BinaryFileReader(std::string const& file_path)
			: stream(file_path, std::ios::binary)
		{
			if (stream.is_open())
				archive = std::make_unique<cereal::BinaryInputArchive>(stream);
		}

		Bool IsValid() const { return archive != nullptr; }

		template<typename... Args>
		void Read(Args&... args)
		{
			ADRIA_ASSERT(IsValid());
			(*archive)(args...);
		}

		void ReadBlob(void* data, Usize size)
		{
			ADRIA_ASSERT(IsValid());
			archive->loadBinary(data, size);
		}

	private:
		std::ifstream stream;
		std::unique_ptr<cereal::BinaryInputArchive> archive;
	};
}
