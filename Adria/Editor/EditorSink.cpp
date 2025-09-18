#include "EditorSink.h"

namespace adria
{

	static ImVec4 GetLevelColor(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::LOG_DEBUG:   return ImVec4(0.5f, 1.0f, 0.5f, 1.0f); // Green
		case LogLevel::LOG_INFO:    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
		case LogLevel::LOG_WARNING: return ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // Yellow
		case LogLevel::LOG_ERROR:   return ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Red
		case LogLevel::LOG_FATAL:   return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Bright Red
		default:					return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
		}
	}

	struct ImGuiLogger
	{
		ImGuiTextBuffer     Buf;
		ImGuiTextFilter     Filter;
		ImVector<Int>       LineOffsets;
		ImVector<ImVec4>    LineColors;   
		Bool                AutoScroll;

		ImGuiLogger()
		{
			AutoScroll = true;
			Clear();
		}

		void Clear()
		{
			Buf.clear();
			LineOffsets.clear();
			LineOffsets.push_back(0);
			LineColors.clear();
		}

		void AddLog(ImVec4 color, Char const* fmt, ...) IM_FMTARGS(2)
		{
			Int old_size = Buf.size();
			va_list args;
			va_start(args, fmt);
			Buf.appendfv(fmt, args);
			va_end(args);

			for (Int new_size = Buf.size(); old_size < new_size; old_size++)
			{
				if (Buf[old_size] == '\n')
				{
					LineOffsets.push_back(old_size + 1);
					LineColors.push_back(color);
				}
			}
		}

		void Draw(Char const* title, Bool* p_open = NULL)
		{
			if (!ImGui::Begin(title, p_open))
			{
				ImGui::End();
				return;
			}

			if (ImGui::BeginPopup("Options"))
			{
				ImGui::Checkbox("Auto-scroll", &AutoScroll);
				ImGui::EndPopup();
			}

			if (ImGui::Button("Options")) ImGui::OpenPopup("Options");
			ImGui::SameLine();
			Bool clear = ImGui::Button("Clear");
			ImGui::SameLine();
			Bool copy = ImGui::Button("Copy");
			ImGui::SameLine();
			Filter.Draw("Filter", -100.0f);

			ImGui::Separator();
			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

			if (clear)
			{
				Clear();
			}
			if (copy)
			{
				ImGui::LogToClipboard();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			Char const* buf = Buf.begin();
			Char const* buf_end = Buf.end();

			if (Filter.IsActive())
			{
				for (Int line_no = 0; line_no < LineOffsets.Size; line_no++)
				{
					Char const* line_start = buf + LineOffsets[line_no];
					Char const* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;

					if (Filter.PassFilter(line_start, line_end))
					{
						ImVec4 color = (line_no < LineColors.Size) ? LineColors[line_no] : ImVec4(1, 1, 1, 1);
						ImGui::PushStyleColor(ImGuiCol_Text, color);
						ImGui::TextUnformatted(line_start, line_end);
						ImGui::PopStyleColor();
					}
				}
			}
			else
			{
				ImGuiListClipper clipper;
				clipper.Begin(LineOffsets.Size);
				while (clipper.Step())
				{
					for (Int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
					{
						Char const* line_start = buf + LineOffsets[line_no];
						Char const* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;

						ImVec4 color = (line_no < LineColors.Size) ? LineColors[line_no] : ImVec4(1, 1, 1, 1);
						ImGui::PushStyleColor(ImGuiCol_Text, color);
						ImGui::TextUnformatted(line_start, line_end);
						ImGui::PopStyleColor();
					}
				}
				clipper.End();
			}

			ImGui::PopStyleVar();

			if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			{
				ImGui::SetScrollHereY(1.0f);
			}

			ImGui::EndChild();
			ImGui::End();
		}
	};


	EditorSink::EditorSink(LogLevel logger_level) : logger_level{ logger_level }, imgui_log(new ImGuiLogger{})
	{
	}
	void EditorSink::Log(LogLevel level, LogChannel channel, Char const* entry, Char const* file, Uint32 line)
	{
		if (level < logger_level)
		{
			return;
		}
		std::string log_entry = GetLogTime() + LevelToString(level) + ChannelToString(channel) + std::string(entry) + "\n";
		imgui_log->AddLog(GetLevelColor(level), log_entry.c_str());
	}
	void EditorSink::Draw(const Char* title, Bool* p_open)
	{
		imgui_log->Draw(title, p_open);
	}
}

