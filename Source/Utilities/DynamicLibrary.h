#pragma once

namespace adria
{
	class DynamicLibrary final
	{
	public:
		DynamicLibrary();
		explicit DynamicLibrary(Char const* filename);
		DynamicLibrary(void* handle);
		ADRIA_NONCOPYABLE_NONMOVABLE(DynamicLibrary)
		DynamicLibrary& operator=(void*);
		~DynamicLibrary();

		Bool IsOpen() const { return lib_handle != nullptr; }
		Bool Open(Char const* libname);
		void Close();

		void* GetSymbolAddress(Char const* name) const;
		template <typename T>
		Bool GetSymbol(Char const* name, T** ptr) const
		{
			*ptr = reinterpret_cast<T*>(GetSymbolAddress(name));
			return *ptr != nullptr;
		}

		static std::string GetUnprefixedFilename(Char const* filename);
		static std::string GetVersionedFilename(Char const* libname, Int major = -1, Int minor = -1);

	private:
		void* lib_handle = nullptr;
	};
}