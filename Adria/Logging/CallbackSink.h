#pragma once
#include "Logging/Log.h"

namespace adria
{
	using LogCallbackT = void(*)(LogLevel, LogChannel, Char const*, Char const*, Uint32);

	class CallbackSink : public ILogSink
	{
	public:
		CallbackSink(LogCallbackT callback, LogLevel log_level = LogLevel::LOG_DEBUG);
		virtual ~CallbackSink() override;
		virtual void Log(LogLevel level, LogChannel channel, Char const* entry, Char const* file, Uint32 line) override;

	private:
		LogCallbackT   log_callback;
		LogLevel const log_level;
	};

}