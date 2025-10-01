#include "DebuggerSink.h"
#include "Platform/Windows/Windows.h"

namespace adria
{
	DebuggerSink::DebuggerSink(LogLevel log_level)
		: log_level{ log_level }
	{}

	DebuggerSink::~DebuggerSink()
	{}

	void DebuggerSink::Log(LogLevel level, LogChannel channel, Char const* entry, Char const* file, Uint32 line)
	{
		if (level < log_level)
		{
			return;
		}
		std::string const log = GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + ChannelToString(channel) + std::string(entry) + "\n";
		OutputDebugStringA(log.c_str());
	}

	void DebuggerSink::Flush()
	{
		
	}

}

