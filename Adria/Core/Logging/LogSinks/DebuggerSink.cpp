#include "DebuggerSink.h"
#include "Core/Windows.h"

namespace adria
{
	DebuggerSink::DebuggerSink(LogLevel log_level)
		: log_level{ log_level }
	{}

	DebuggerSink::~DebuggerSink()
	{}

	void DebuggerSink::Log(LogLevel level, Char const* entry, Char const* file, Uint32 line)
	{
		if (level < log_level)
		{
			return;
		}
		std::string log = GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + std::string(entry) + "\n";
		OutputDebugStringA(log.c_str());
	}

	void DebuggerSink::Flush()
	{
		
	}

}

