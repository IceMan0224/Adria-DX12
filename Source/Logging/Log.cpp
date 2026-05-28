#include <ctime>
#include <mutex>
#include <thread>
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
			std::lock_guard<std::mutex> guard(sinks_mutex);
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
			{
				std::lock_guard<std::mutex> guard(sinks_mutex);
				for (auto& logger : log_sinks)
				{
					logger->Flush();
				}
			}
			pause.store(false);
		}

		std::vector<std::unique_ptr<ILogSink>> log_sinks;
		std::mutex sinks_mutex;
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
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}

				Bool success = log_queue.TryPop(entry);
				if (success)
				{
					std::lock_guard<std::mutex> guard(sinks_mutex);
					for (auto& log_sink : log_sinks)
					{
						if (log_sink)
						{
							log_sink->Log(entry.level, entry.channel, entry.str.c_str(), entry.filename.c_str(), entry.line);
						}
					}
				}
				else
				{
					if (exit.load() && log_queue.Empty())
					{
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		}
	};

	std::string LevelToString(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Debug:
			return "[DEBUG]";
		case LogLevel::Info:
			return "[INFO]";
		case LogLevel::Warning:
			return "[WARNING]";
		case LogLevel::Error:
			return "[ERROR]";
		case LogLevel::Fatal:
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
		std::tm tm_buf{};
#if defined(_WIN32)
		localtime_s(&tm_buf, &c_time);
#else
		localtime_r(&c_time, &tm_buf);
#endif
		Char buf[64];
		std::strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Y", &tm_buf);
		return std::string("[") + buf + "]";
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

	LogManager& GetLogManager()
	{
		static LogManager log_manager;
		return log_manager;
	}
}
