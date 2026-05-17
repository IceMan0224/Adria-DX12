#include "SystemInfo.h"
#include <thread>

#if defined(_WIN32)
#include <intrin.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <fstream>
#endif

namespace adria
{
	std::string GetCpuName()
	{
#if defined(_WIN32)
		// CPUID leaves 0x80000002-0x80000004 return the brand string in 48 bytes.
		Int32 regs[4]{};
		Char  brand[0x40]{};
		__cpuid(regs, 0x80000000);
		if ((Uint32)regs[0] < 0x80000004u) return "Unknown CPU";
		for (Uint32 leaf = 0x80000002u; leaf <= 0x80000004u; ++leaf)
		{
			__cpuid(regs, (Int32)leaf);
			std::memcpy(brand + (leaf - 0x80000002u) * 16u, regs, 16);
		}
		std::string name(brand);
		// Trim leading/trailing whitespace.
		auto first = name.find_first_not_of(' ');
		auto last  = name.find_last_not_of(' ');
		return (first == std::string::npos) ? "Unknown CPU" : name.substr(first, last - first + 1);
#elif defined(__APPLE__)
		Char   buffer[256]{};
		size_t size = sizeof(buffer);
		if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0)
		{
			return std::string(buffer);
		}
		return "Unknown CPU";
#elif defined(__linux__)
		std::ifstream cpuinfo("/proc/cpuinfo");
		std::string line;
		while (std::getline(cpuinfo, line))
		{
			if (line.rfind("model name", 0) == 0)
			{
				auto colon = line.find(':');
				if (colon != std::string::npos)
				{
					auto start = line.find_first_not_of(" \t", colon + 1);
					return start == std::string::npos ? "Unknown CPU" : line.substr(start);
				}
			}
		}
		return "Unknown CPU";
#else
		return "Unknown CPU";
#endif
	}

	Uint32 GetLogicalCoreCount()
	{
		Uint32 n = std::thread::hardware_concurrency();
		return n == 0 ? 1u : n;
	}
}
