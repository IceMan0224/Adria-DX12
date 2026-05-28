#pragma once
#include <mutex>

namespace adria
{
	template <typename T>
	class Singleton
	{
	public:
		ADRIA_NONCOPYABLE(Singleton)

		static Bool HasInstance() { return Instance != nullptr; }
		static T& Get()
		{
			std::lock_guard<std::mutex> lock(InstanceMutex);
			if (Instance != nullptr)
			{
				return *Instance;
			}
			IsInitializing = true;
			Instance = new T();
			IsInitializing = false;
			return *Instance;
		};
		static void Reset()
		{
			std::lock_guard<std::mutex> lock(InstanceMutex);
			ADRIA_ASSERT_MSG(Instance != nullptr, "Attempting to destroy a singleton before it has been instantiated");
			IsDestroying = true;
			delete Instance;
			Instance = nullptr;
			IsDestroying = false;
		};

	protected:
		Singleton()
		{
			ADRIA_ASSERT_MSG(IsInitializing, "Singleton not properly initialized. Did you forget to call Get()?");
		};

		~Singleton()
		{
			if (Instance != nullptr && !IsDestroying)
			{
				Reset();
			}
		};

	protected:
		static T* Instance;
		static Bool IsInitializing;
		static Bool IsDestroying;
		static std::mutex InstanceMutex;
	};

	template <typename T>
	T* Singleton<T>::Instance = nullptr;

	template <typename T>
	Bool Singleton<T>::IsInitializing = false;

	template <typename T>
	Bool Singleton<T>::IsDestroying = false;

	template <typename T>
	std::mutex Singleton<T>::InstanceMutex;
}