#pragma once

namespace adria
{
	class ConsoleLogger : public ILogger
	{
	public:
		ConsoleLogger(Bool use_cerr = false, LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~ConsoleLogger() override;
		ADRIA_NONCOPYABLE_NONMOVABLE(ConsoleLogger)

		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
		virtual void Flush() override;

	private:
		Bool const use_cerr;
		LogLevel const logger_level;
	};

} 