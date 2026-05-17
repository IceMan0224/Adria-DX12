#if defined(_WIN32)
#include <shellapi.h>
#endif
#include "CLIParser.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(CommandLine);

	CLIParseResult::CLIParseResult(std::vector<CLIArg> const& args, std::unordered_map<std::string, Uint32> const& prefix_index_map)
	{
		for (auto const& [prefix, index] : prefix_index_map)
		{
			cli_arg_map[prefix] = args[index];
		}
	}


	CLIParseResult CLIParser::Parse(Int argc, Char** argv)
	{
		std::vector<std::string> cmd_line(argv, argv + argc);
		return ParseImpl(cmd_line);
	}

	CLIParseResult CLIParser::Parse(std::wstring const& cmd_line)
	{
#if defined(_WIN32)
		Int argc;
		Wchar** argv = CommandLineToArgvW(cmd_line.c_str(), &argc);
		return Parse(argc, argv);
#else
		ADRIA_ASSERT_MSG(false, "Wide string command line parsing is only supported on Windows!");
		return ParseImpl({});
#endif
	}

	CLIParseResult CLIParser::Parse(Int argc, Wchar** argv)
	{
#if defined(_WIN32)
		std::vector<std::wstring> wide_cmd_line(argv, argv + argc);
		std::vector<std::string> cmd_line; cmd_line.reserve(argc);
		for (std::wstring const& wide_arg : wide_cmd_line)
		{
			cmd_line.push_back(ToString(wide_arg));
		}
		return ParseImpl(cmd_line);
#else
		ADRIA_ASSERT_MSG(false, "Wide string command line parsing is only supported on Windows!");
		return ParseImpl({});
#endif
	}

	CLIParseResult CLIParser::ParseImpl(std::vector<std::string> const& cmd_line)
	{
		std::unordered_map<std::string, CLIArg> cli_arg_map;
		for (Int i = 0; i < cmd_line.size(); ++i)
		{
			std::string const& arg = cmd_line[i];
			if (prefix_arg_index_map.find(arg) != prefix_arg_index_map.end())
			{
				Uint32 arg_index = prefix_arg_index_map[arg];
				CLIArg& cli_arg = args[arg_index];
				cli_arg.SetIsPresent();
				if (cli_arg.has_value)
				{
					while (i + 1 < cmd_line.size() && !prefix_arg_index_map.contains(cmd_line[i + 1]))
					{
						cli_arg.AddValue(cmd_line[++i]);
					}
					if (cli_arg.values.empty())
					{
						ADRIA_WARNING("Missing value for cmdline argument %s", cli_arg.prefixes[0].c_str());
					}
				}
			}
		}

		for (CLIArg const& arg : args)
		{
			for (std::string const& prefix : arg.prefixes)
			{
				cli_arg_map[prefix] = arg;
			}
		}

		return CLIParseResult(args, prefix_arg_index_map);
	}

}

