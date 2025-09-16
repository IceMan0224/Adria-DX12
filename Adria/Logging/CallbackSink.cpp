#include "CallbackSink.h"

namespace adria
{

	CallbackSink::CallbackSink(LogCallbackT callback, LogLevel log_level) : log_level(log_level)
	{
	}

	CallbackSink::~CallbackSink() {}

	void CallbackSink::Log(LogLevel level, LogChannel channel, Char const* entry, Char const* file, Uint32 line)
	{
		if (level < log_level || log_callback == nullptr)
		{
			return;
		}
		log_callback(level, channel, entry, file, line);
	}
}

