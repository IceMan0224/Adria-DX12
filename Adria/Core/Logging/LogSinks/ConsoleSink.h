#pragma once
#include "Core/Logging/Log.h"

namespace adria
{
	class ConsoleSink : public ILogSink
	{
	public:
		ConsoleSink(Bool use_cerr = false, LogLevel log_level = LogLevel::LOG_DEBUG);
		virtual ~ConsoleSink() override;
		ADRIA_NONCOPYABLE_NONMOVABLE(ConsoleSink)

		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
		virtual void Flush() override;

	private:
		Bool const use_cerr;
		LogLevel const log_level;
	};

} 