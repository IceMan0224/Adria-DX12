#include "DynamicLibrary.h"
#include <format>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace adria
{
	DynamicLibrary::DynamicLibrary() = default;

	DynamicLibrary::DynamicLibrary(Char const* filename)
	{
		Open(filename);
	}

	DynamicLibrary::DynamicLibrary(void* handle)
	{
		lib_handle = handle;
	}

	DynamicLibrary::~DynamicLibrary()
	{
		Close();
	}

	DynamicLibrary& DynamicLibrary::operator=(void* handle)
	{
		lib_handle = handle;
		return *this;
	}

	std::string DynamicLibrary::GetUnprefixedFilename(Char const* filename)
	{
#if defined(_WIN32)
		return std::string(filename) + ".dll";
#elif defined(__APPLE__)
		return std::string(filename) + ".dylib";
#else
		return std::string(filename) + ".so";
#endif
	}

	std::string DynamicLibrary::GetVersionedFilename(Char const* libname, Int major, Int minor)
	{
#if defined(_WIN32)
		if (major >= 0 && minor >= 0)
		{
			return std::format("{}-{}-{}.dll", libname, major, minor);
		}
		else if (major >= 0)
		{
			return std::format("{}-{}.dll", libname, major);
		}
		else
		{
			return std::format("{}.dll", libname);
		}
#elif defined(__APPLE__)
		Char const* prefix = std::strncmp(libname, "lib", 3) ? "lib" : "";
		if (major >= 0 && minor >= 0)
		{
			return std::format("{}{}.{}.{}.dylib", prefix, libname, major, minor);
		}
		else if (major >= 0)
		{
			return std::format("{}{}.{}.dylib", prefix, libname, major);
		}
		else
		{
			return std::format("{}{}.dylib", prefix, libname);
		}
#else
		Char const* prefix = std::strncmp(libname, "lib", 3) ? "lib" : "";
		if (major >= 0 && minor >= 0)
		{
			return std::format("{}{}.so.{}.{}", prefix, libname, major, minor);
		}
		else if (major >= 0)
		{
			return std::format("{}{}.so.{}", prefix, libname, major);
		}
		else
		{
			return std::format("{}{}.so", prefix, libname);
		}
#endif
	}

	Bool DynamicLibrary::Open(Char const* filename)
	{
#ifdef _WIN32
		lib_handle = reinterpret_cast<void*>(LoadLibraryA(filename));
#else
		lib_handle = dlopen(filename, RTLD_NOW);
#endif
		return lib_handle != nullptr;
	}

	void DynamicLibrary::Close()
	{
		if (!IsOpen())
		{
			return;
		}

#ifdef _WIN32
		FreeLibrary(static_cast<HMODULE>(lib_handle));
#else
		dlclose(lib_handle);
#endif
		lib_handle = nullptr;
	}

	void* DynamicLibrary::GetSymbolAddress(Char const* name) const
	{
#ifdef _WIN32
		return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib_handle), name));
#else
		return reinterpret_cast<void*>(dlsym(lib_handle, name));
#endif
	}
}

