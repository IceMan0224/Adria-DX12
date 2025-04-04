#include "Log.h"
#include <chrono>
#include <ctime>   
#include <vector>
#include <thread>
#include "Utilities/ConcurrentQueue.h"

namespace adria
{
	struct QueueEntry
	{
		LogLevel level;
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

		void RegisterLogger(ILogger* logger)
		{
			loggers.emplace_back(logger);
		}
		ILogger* GetLastLogger() const
		{
			return loggers.back().get();
		}
		void Log(LogLevel level, Char const* str, Char const* filename, Uint32 line)
		{
			log_queue.Push(QueueEntry{ level, str, filename, line });
		}
		void LogSync(LogLevel level, Char const* str, Char const* filename, Uint32 line)
		{
			for (auto& logger : loggers)
			{
				if (logger)
				{
					logger->Log(level, str, filename, line);
				}
			}
		}
		void Flush()
		{
			pause.store(true);
			for (auto& logger : loggers) logger->Flush();
			pause.store(false);
		}

		std::vector<std::unique_ptr<ILogger>> loggers;
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
				if (pause.load()) continue;

				Bool success = log_queue.TryPop(entry);
				if (success)
				{
					for (auto& logger : loggers)
					{
						if (logger)
						{
							logger->Log(entry.level, entry.str.c_str(), entry.filename.c_str(), entry.line);
						}
					}
				}
				if (exit.load() && log_queue.Empty()) break;
			}
		}
	};

	std::string LevelToString(LogLevel type)
	{
		switch (type)
		{
		case LogLevel::LOG_DEBUG:
			return "[DEBUG]";
		case LogLevel::LOG_INFO:
			return "[INFO]";
		case LogLevel::LOG_WARNING:
			return "[WARNING]";
		case LogLevel::LOG_ERROR:
			return "[ERROR]";
		}
		return "";
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

	void LogManager::Register(ILogger* logger)
	{
		pimpl->RegisterLogger(logger);
	}

	ILogger* LogManager::GetLastLogger()
	{
		return pimpl->GetLastLogger();
	}

	void LogManager::Log(LogLevel level, Char const* str, Char const* filename, Uint32 line)
	{
		pimpl->Log(level, str, filename, line);
	}

	void LogManager::LogSync(LogLevel level, Char const* str, Char const* filename, Uint32 line)
	{
		pimpl->LogSync(level, str, filename, line);
	}

	void LogManager::Flush()
	{
		pimpl->Flush();
	}

}