#include <ctime>
#include "Log.h"
#include "Utilities/ConcurrentQueue.h"

namespace adria
{
	struct QueueEntry
	{
		LogLevel level;
		LogChannel channel;
		std::string str;
		std::string filename;
		Uint32 line;
	};

	class LogManagerImpl
	{
	public:

		LogManagerImpl() : log_thread(&LogManagerImpl::ProcessLogs, this) {}
		~LogManagerImpl()
		{
			exit.store(true);
			log_thread.join();
		}

		void RegisterLogger(ILogSink* logger)
		{
			log_sinks.emplace_back(logger);
		}
		ILogSink* GetLastSink() const
		{
			return log_sinks.back().get();
		}
		void Log(LogLevel level, LogChannel channel, Char const* str, Char const* filename, Uint32 line)
		{
			log_queue.Push(QueueEntry{ level, channel, str, filename, line });
		}
		void LogSync(LogLevel level, LogChannel channel, Char const* str, Char const* filename, Uint32 line)
		{
			for (auto& log_sink : log_sinks)
			{
				if (log_sink)
				{
					log_sink->Log(level, channel, str, filename, line);
				}
			}
		}
		void Flush()
		{
			pause.store(true);
			for (auto& logger : log_sinks)
			{
				logger->Flush();
			}
			pause.store(false);
		}

		std::vector<std::unique_ptr<ILogSink>> log_sinks;
		ConcurrentQueue<QueueEntry> log_queue;
		std::thread log_thread;
		std::atomic_bool exit = false;
		std::atomic_bool pause = false;

	private:
		void ProcessLogs()
		{
			QueueEntry entry{};
			while (true)
			{
				if (pause.load())
				{
					continue;
				}

				Bool success = log_queue.TryPop(entry);
				if (success)
				{
					for (auto& log_sink : log_sinks)
					{
						if (log_sink)
						{
							log_sink->Log(entry.level, entry.channel, entry.str.c_str(), entry.filename.c_str(), entry.line);
						}
					}
				}
				if (exit.load() && log_queue.Empty())
				{
					break;
				}
			}
		}
	};

	std::string LevelToString(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::LOG_DEBUG:
			return "[DEBUG]";
		case LogLevel::LOG_INFO:
			return "[INFO]";
		case LogLevel::LOG_WARNING:
			return "[WARNING]";
		case LogLevel::LOG_ERROR:
			return "[ERROR]";
		case LogLevel::LOG_FATAL:
			return "[FATAL]";
		}
		return "[UNKNOWN]";
	}

	std::string ChannelToString(LogChannel channel)
	{
		static std::string LogChannelNames[] =
		{
			#define LOG_CHANNEL(X) "["#X"] ",
			#include "LogChannels.def"
			#undef LOG_CHANNEL
		};
		static_assert(std::size(LogChannelNames) == (Uint32)LogChannel::MaxCount);
		return LogChannelNames[(Uint8)channel];
	}

	std::string GetLogTime()
	{
		auto time = std::chrono::system_clock::now();
		time_t c_time = std::chrono::system_clock::to_time_t(time);
		std::string time_str = std::string(ctime(&c_time));
		time_str.pop_back();
		return "[" + time_str + "]";
	}
	std::string LineInfoToString(Char const* file, Uint32 line)
	{
		return "[File: " + std::string(file) + "  Line: " + std::to_string(line) + "]";
	}

	LogManager::LogManager() : pimpl(new LogManagerImpl) {}
	LogManager::~LogManager() = default;

	void LogManager::Register(ILogSink* logger)
	{
		pimpl->RegisterLogger(logger);
	}

	ILogSink* LogManager::GetLastSink()
	{
		return pimpl->GetLastSink();
	}

	void LogManager::Log(LogLevel level, LogChannel channel, Char const* str, Char const* filename, Uint32 line)
	{
		pimpl->Log(level, channel, str, filename, line);
	}

	void LogManager::LogSync(LogLevel level, LogChannel channel, Char const* str, Char const* filename, Uint32 line)
	{
		pimpl->LogSync(level, channel, str, filename, line);
	}

	void LogManager::Flush()
	{
		pimpl->Flush();
	}

}
