#pragma once

namespace adria
{

	class DebuggerSink : public ILogSink
	{
	public:
		DebuggerSink(LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual ~DebuggerSink() override;
		virtual void Log(LogLevel level, Char const* entry, Char const* file, uint32_t line) override;
		virtual void Flush() override;

	private:
		LogLevel const logger_level;
	};

}