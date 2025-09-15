#include "ConsoleSink.h"

namespace adria
{
	ConsoleSink::ConsoleSink(Bool use_cerr, LogLevel log_level)
		: use_cerr{ use_cerr }, log_level{ log_level }
	{
	}

	ConsoleSink::~ConsoleSink()
	{
		Flush(); 
	}

	void ConsoleSink::Log(LogLevel level, Char const* entry, Char const* file, Uint32 line)
	{
		if (level < log_level)
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

	void ConsoleSink::Flush()
	{
		FILE* target_stream = use_cerr ? stderr : stdout;
		fflush(target_stream);
	}
} 