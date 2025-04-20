#include "EditorSink.h"

namespace adria
{
	struct ImGuiLogger
	{
		ImGuiTextBuffer     Buf;
		ImGuiTextFilter     Filter;
		ImVector<Int>       LineOffsets;
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
		}
		void AddLog(const Char* fmt, ...) IM_FMTARGS(2)
		{
			Int old_size = Buf.size();
			va_list args;
			va_start(args, fmt);
			Buf.appendfv(fmt, args);
			va_end(args);
			for (Int new_size = Buf.size(); old_size < new_size; old_size++)
				if (Buf[old_size] == '\n')
					LineOffsets.push_back(old_size + 1);
		}
		void Draw(const Char* title, Bool* p_open = NULL)
		{
			if (!ImGui::Begin(title, p_open))
			{
				ImGui::End();
				return;
			}

			// Options menu
			if (ImGui::BeginPopup("Options"))
			{
				ImGui::Checkbox("Auto-scroll", &AutoScroll);
				ImGui::EndPopup();
			}

			// Main window
			if (ImGui::Button("Options"))
				ImGui::OpenPopup("Options");
			ImGui::SameLine();
			Bool clear = ImGui::Button("Clear");
			ImGui::SameLine();
			Bool copy = ImGui::Button("Copy");
			ImGui::SameLine();
			Filter.Draw("Filter", -100.0f);

			ImGui::Separator();
			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

			if (clear)
				Clear();
			if (copy)
				ImGui::LogToClipboard();

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			const Char* buf = Buf.begin();
			const Char* buf_end = Buf.end();
			if (Filter.IsActive())
			{
				for (Int line_no = 0; line_no < LineOffsets.Size; line_no++)
				{
					const Char* line_start = buf + LineOffsets[line_no];
					const Char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
					if (Filter.PassFilter(line_start, line_end))
						ImGui::TextUnformatted(line_start, line_end);
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
						const Char* line_start = buf + LineOffsets[line_no];
						const Char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
						ImGui::TextUnformatted(line_start, line_end);
					}
				}
				clipper.End();
			}
			ImGui::PopStyleVar();

			if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);

			ImGui::EndChild();
			ImGui::End();
		}
	};

	EditorSink::EditorSink(LogLevel logger_level) : logger_level{ logger_level }, imgui_log(new ImGuiLogger{})
	{
	}
	void EditorSink::Log(LogLevel level, Char const* entry, Char const* file, Uint32 line)
	{
		if (level < logger_level) return;
		std::string log_entry = GetLogTime() + LevelToString(level) + std::string(entry) + "\n";
		imgui_log->AddLog(log_entry.c_str());
	}
	void EditorSink::Draw(const Char* title, Bool* p_open)
	{
		imgui_log->Draw(title, p_open);
	}
}

