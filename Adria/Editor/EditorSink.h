#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

namespace adria
{
	class EditorSink : public ILogSink
	{
	public:
		EditorSink(LogLevel logger_level = LogLevel::LOG_DEBUG);
		virtual void Log(LogLevel level, Char const* entry, Char const* file, Uint32 line) override;
		void Draw(const Char* title, Bool* p_open = nullptr);

	private:
		std::unique_ptr<struct ImGuiLogger> imgui_log;
		LogLevel logger_level;
	};

}