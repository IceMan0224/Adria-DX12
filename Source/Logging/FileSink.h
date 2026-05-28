#pragma once
#include "Logging/Log.h"

namespace adria
{
	class FileSink : public ILogSink
	{
	public:
		FileSink(Char const* log_file, LogLevel log_level = LogLevel::Debug, Bool append_mode = false);
		virtual ~FileSink() override;
		ADRIA_NONCOPYABLE_NONMOVABLE(FileSink)

		virtual void Log(LogLevel level, LogChannel channel, Char const* entry, Char const* file, Uint32 line) override;
		virtual void Flush() override;

	private:
		FILE* log_handle = nullptr;
		LogLevel const log_level;
	};
}