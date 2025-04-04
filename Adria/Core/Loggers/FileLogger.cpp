#include "FileLogger.h"
#include "Core/Paths.h"      

namespace adria
{

	FileLogger::FileLogger(Char const* log_file, LogLevel logger_level, bool append_mode)
		: logger_level{ logger_level }
	{
		std::string full_path = paths::LogDir + log_file;
		Char const* mode = append_mode ? "a" : "w";
		log_handle = fopen(full_path.c_str(), mode);
	}

	FileLogger::~FileLogger()
	{
		if (log_handle)
		{
			fflush(log_handle); 
			fclose(log_handle);
			log_handle = nullptr; 
		}
	}

	void FileLogger::Log(LogLevel level, Char const* entry, Char const* file, uint32_t line)
	{
		if (level < logger_level || !log_handle)
		{
			return;
		}

		std::string time_str = GetLogTime();
		std::string line_info_str = LineInfoToString(file, line);
		std::string level_str = LevelToString(level);
		fprintf(log_handle, "%s%s%s%s\n",
			time_str.c_str(),
			line_info_str.c_str(),
			level_str.c_str(),
			entry); 
	}

	void FileLogger::Flush()
	{
		if (log_handle)
		{
			fflush(log_handle); 
		}
	}

} 