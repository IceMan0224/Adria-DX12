#pragma once
#include "Core/Logging/Log.h"

namespace adria
{
	class DebuggerSink : public ILogSink
	{
	public:
		DebuggerSink(LogLevel log_level = LogLevel::LOG_DEBUG);
		virtual ~DebuggerSink() override;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
		virtual void Flush() override;

	private:
		LogLevel const log_level;
	};

}