#pragma once
#include <cstdio>

namespace adria
{
	class FileLogger : public ILogger
	{
	public:
		FileLogger(Char const* log_file, LogLevel logger_level = LogLevel::LOG_DEBUG, Bool append_mode = false);
		virtual ~FileLogger() override;
		ADRIA_NONCOPYABLE_NONMOVABLE(FileLogger)

		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
		virtual void Flush() override;

	private:
		FILE* log_handle = nullptr;
		LogLevel const logger_level;
	};
}