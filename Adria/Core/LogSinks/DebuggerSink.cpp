#include "DebuggerSink.h"
#include "Core/Windows.h"

namespace adria
{
	DebuggerSink::DebuggerSink(LogLevel logger_level /*= ELogLevel::LOG_DEBUG*/)
		: logger_level{ logger_level }
	{}

	DebuggerSink::~DebuggerSink()
	{}

	void DebuggerSink::Log(LogLevel level, Char const* entry, Char const* file, uint32_t line)
	{
		std::string log = GetLogTime() + LineInfoToString(file, line) + LevelToString(level) + std::string(entry) + "\n";
		OutputDebugStringA(log.c_str());
	}

	void DebuggerSink::Flush()
	{
		
	}

}

