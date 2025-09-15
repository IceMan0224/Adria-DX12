#pragma once
#include "Logging/Log.h"

namespace adria
{
	using LogCallbackT = void(*)(LogLevel level, Char const* entry, Char const* file, Uint32 line);

	class CallbackSink : public ILogSink
	{
	public:
		CallbackSink(LogCallbackT callback, LogLevel log_level = LogLevel::LOG_DEBUG);
		virtual ~CallbackSink() override;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;

	private:
		LogCallbackT   log_callback;
		LogLevel const log_level;
	};

}