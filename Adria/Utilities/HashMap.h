#pragma once
#if _DEBUG
#include <unordered_map>
#else 
#include "tsl/robin_map.h"
#endif

namespace adria
{
#if _DEBUG
	template<typename K, typename V>
	using HashMap = std::unordered_map<K, V>;
#else 
	template<typename K, typename V>
	using HashMap = tsl::robin_map<K, V>;
#endif
}