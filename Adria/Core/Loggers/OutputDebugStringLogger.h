#pragma once

namespace adria
{

	class OutputDebugStringLogger : public ILogger
	{
	public:
		OutputDebugStringLogger(LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~OutputDebugStringLogger() override;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, uint32_t line) override;
		virtual void Flush() override;

	private:
		LogLevel const logger_level;
	};

}