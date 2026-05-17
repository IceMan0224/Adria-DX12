#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

namespace adria
{
	class EditorConsole
	{
	public:
		EditorConsole();
		~EditorConsole();

		void Draw(Char const* title, Bool* p_open = nullptr);
		void DrawBasic(Char const* title, Bool* p_open = nullptr);

	private:
		Char                  InputBuf[256];
		ImVector<Char*>       Items;
		ImVector<Char const*> Commands;
		ImVector<Char const*> CommandDescriptions;
		ImVector<Char*>       History;
		Int                   HistoryPos;
		ImGuiTextFilter       Filter;
		Bool                  AutoScroll;
		Bool                  ScrollToBottom;
		Int					  CursorPos;

	private:
		void    ClearLog();
		void    AddLog(Char const* fmt, ...) IM_FMTARGS(2);

		void ExecCommand(Char const* cmd);
		Int	 TextEditCallback(ImGuiInputTextCallbackData* data);
	};
}

