#include "FileSink.h"
#include "Core/Paths.h" 

namespace fs = std::filesystem;

namespace adria
{

	FileSink::FileSink(Char const* log_file, LogLevel log_level, Bool append_mode)
		: log_level{ log_level }
	{
		fs::path fullLogPath = fs::path(paths::LogDir) / log_file;
		fs::path directoryPath = fullLogPath.parent_path();
		if (!directoryPath.empty() && !fs::exists(directoryPath))
		{
			std::error_code ec;
			if (!fs::create_directories(directoryPath, ec))
			{
				return;
			}
		}

		Char const* mode = append_mode ? "a" : "w";
		log_handle = fopen(fullLogPath.string().c_str(), mode);
	}

	FileSink::~FileSink()
	{
		if (log_handle)
		{
			fflush(log_handle); 
			fclose(log_handle);
			log_handle = nullptr; 
		}
	}

	void FileSink::Log(LogLevel level, LogChannel channel, Char const* entry, Char const* file, Uint32 line)
	{
		if (level < log_level || !log_handle)
		{
			return;
		}

		std::string const time_str = GetLogTime();
		std::string const line_info_str = LineInfoToString(file, line);
		std::string const level_str = LevelToString(level);
		std::string const channel_str = ChannelToString(channel);

		fprintf(log_handle, "%s%s%s%s%s\n",
			time_str.c_str(),
			line_info_str.c_str(),
			level_str.c_str(),
			channel_str.c_str(),
			entry); 
	}

	void FileSink::Flush()
	{
		if (log_handle)
		{
			fflush(log_handle); 
		}
	}

} 