#include "ConsoleLogger.h"
#include <cstdio>             

namespace adria
{

	ConsoleLogger::ConsoleLogger(Bool use_cerr, LogLevel logger_level)
		: use_cerr{ use_cerr }, logger_level{ logger_level }
	{
	}

	ConsoleLogger::~ConsoleLogger()
	{
		Flush(); 
	}

	void ConsoleLogger::Log(LogLevel level, Char const* entry, Char const* file, uint32_t line)
	{
		if (level < logger_level)
		{
			return;
		}
		FILE* target_stream = use_cerr ? stderr : stdout;
		std::string time_str = GetLogTime();
		std::string line_info_str = LineInfoToString(file, line);
		std::string level_str = LevelToString(level);

		fprintf(target_stream, "%s%s%s%s\n",
			time_str.c_str(),
			line_info_str.c_str(),
			level_str.c_str(),
			entry); 
	}

	void ConsoleLogger::Flush()
	{
		FILE* target_stream = use_cerr ? stderr : stdout;
		fflush(target_stream);
	}
} 