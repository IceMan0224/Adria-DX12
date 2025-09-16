#pragma once

namespace adria
{
	template<typename T>
	constexpr Bool IsAligned(T value, Uint64 alignment)
	{
		return (value % static_cast<T>(alignment)) == 0;
	}

	template<typename T>
	constexpr T AlignUp(T value, Uint64 alignment)
	{
		return (value + static_cast<T>(alignment - 1)) / static_cast<T>(alignment) * static_cast<T>(alignment);
	}

	template<typename T>
	constexpr T AlignDown(T value, Uint64 alignment)
	{
		return value / static_cast<T>(alignment) * static_cast<T>(alignment);
	}

	template<typename T>
	constexpr bool IsAlignedPow2(T value, Uint64 alignment)
	{
		return (value & static_cast<T>(alignment - 1)) == 0;
	}

	template<typename T>
	constexpr T AlignUpPow2(T value, Uint64 alignment)
	{
		return (value + static_cast<T>(alignment - 1)) & static_cast<T>(~static_cast<T>(alignment - 1));
	}

	template<typename T>
	constexpr T AlignDownPow2(T value, Uint64 alignment)
	{
		return value & static_cast<T>(~static_cast<T>(alignment - 1));
	}

	template<typename T>
	constexpr bool IsPow2(T value)
	{
		return (value & (value - 1)) == 0;
	}

	template<typename T>
	constexpr T PreviousPow2(T value)
	{
		if (value == static_cast<T>(0))
		{
			return 0;
		}

		value |= (value >> 1);
		value |= (value >> 2);
		value |= (value >> 4);

		if constexpr (sizeof(T) >= 16)
		{
			value |= (value >> 8);
		}
		if constexpr (sizeof(T) >= 32)
		{
			value |= (value >> 16);
		}
		if constexpr (sizeof(T) >= 64)
		{
			value |= (value >> 32);
		}
		return value - (value >> 1);
	}

	template<typename T>
	constexpr T NextPow2(T value)
	{
		if (value == static_cast<T>(0))
		{
			return 0;
		}

		value--;
		value |= (value >> 1);
		value |= (value >> 2);
		value |= (value >> 4);

		if constexpr (sizeof(T) >= 16)
		{
			value |= (value >> 8);
		}
		if constexpr (sizeof(T) >= 32)
		{
			value |= (value >> 16);
		}
		if constexpr (sizeof(T) >= 64)
		{
			value |= (value >> 32);
		}
		value++;
		return value;
	}

	inline constexpr Uint64 INVALID_ALLOC_OFFSET = static_cast<Uint64>(-1);

	struct DummyMutex
	{
		void lock() {}
		void unlock() {}
	};

	template<typename AllocatorT, typename T>
	concept IsAllocator = requires(AllocatorT a, void* ptr, Uint64 n, Uint64 align)
	{
		{ a.Allocate(n, align) } -> std::same_as<void*>;   // Generic allocate returning void*
		{ a.template Allocate<T>(n) } -> std::same_as<T*>; // Typed allocate returning T*
		{ a.Deallocate(ptr, n) };
	};

}