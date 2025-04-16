#pragma once
#include <cstdio>

namespace adria
{
	class FileSink : public ILogSink
	{
	public:
		FileSink(Char const* log_file, LogLevel logger_level = LogLevel::LOG_DEBUG, Bool append_mode = false);
		virtual ~FileSink() override;
		ADRIA_NONCOPYABLE_NONMOVABLE(FileSink)

		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
		virtual void Flush() override;

	private:
		FILE* log_handle = nullptr;
		LogLevel const logger_level;
	};
}