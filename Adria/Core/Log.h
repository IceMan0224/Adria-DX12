#pragma once
#include <memory> 
#include <string>
#include <source_location>

namespace adria
{

	enum class LogLevel : Uint8
	{
		LOG_DEBUG,
		LOG_INFO,
		LOG_WARNING,
		LOG_ERROR
	};

	std::string LevelToString(LogLevel type);
	std::string GetLogTime();
	std::string LineInfoToString(Char const* file, Uint32 line);

	class ILogger
	{
	public:
		virtual ~ILogger() = default;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) = 0;
		virtual void Flush() {}
	};

	class LogManager
	{
	public:
		LogManager();
		ADRIA_NONCOPYABLE_NONMOVABLE(LogManager)
		~LogManager();

		template<typename LoggerT, typename... Args> requires std::is_base_of_v<ILogger, LoggerT>
		ADRIA_MAYBE_UNUSED LoggerT* Register(Args&&... args)
		{
			Register(new LoggerT(std::forward<Args>(args)...));
			return static_cast<LoggerT*>(GetLastLogger());
		}
		void Log(LogLevel level, Char const* str, Char const* file, Uint32 line);
		void LogSync(LogLevel level, Char const* str, Char const* file, Uint32 line);
		void Flush();

	private:
		std::unique_ptr<class LogManagerImpl> pimpl;

	private:
		void Register(ILogger* logger);
		ILogger* GetLastLogger();
	};
	inline LogManager g_Log{};

	#define ADRIA_LOG(level, ... ) [&]()  \
	{ \
		Uint64 const size = snprintf(nullptr, 0, __VA_ARGS__) + 1; \
		std::unique_ptr<Char[]> buf = std::make_unique<Char[]>(size); \
		snprintf(buf.get(), size, __VA_ARGS__); \
		g_Log.Log(LogLevel::LOG_##level, buf.get(), __FILE__, __LINE__);  \
	}()
	#define ADRIA_DEBUG(...)	ADRIA_LOG(DEBUG, __VA_ARGS__)
	#define ADRIA_INFO(...)		ADRIA_LOG(INFO, __VA_ARGS__)
	#define ADRIA_WARNING(...)  ADRIA_LOG(WARNING, __VA_ARGS__)
	#define ADRIA_ERROR(...)	ADRIA_LOG(ERROR, __VA_ARGS__)

	#define ADRIA_LOG_SYNC(level, ... ) [&]()  \
	{ \
		Uint64 const size = snprintf(nullptr, 0, __VA_ARGS__) + 1; \
		std::unique_ptr<Char[]> buf = std::make_unique<Char[]>(size); \
		snprintf(buf.get(), size, __VA_ARGS__); \
		g_Log.LogSync(LogLevel::LOG_##level, buf.get(), __FILE__, __LINE__);  \
	}()
	#define ADRIA_LOG_FLUSH()   (g_Log.Flush())
	#define ADRIA_LOGGER(LogClass, ...) g_Log.Register<LogClass>(__VA_ARGS__);
}