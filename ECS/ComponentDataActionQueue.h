#pragma once

/// The following functions are all UNSAFE to use on components themselves.
/// They're meant to be used on pointer data that components may contain.

#include <vector>
#include <functional>

namespace au {
	namespace detail {
		struct IDestroyComponentData {
			virtual void Destroy() = 0;
		};

		template<typename T>
		struct DestroyComponentData : public IDestroyComponentData {
			T* data;
			DestroyComponentData(T* v) : data(v) {}
			void Destroy() override { delete data; }
		};

		template<typename T>
		struct DestroyComponentDataArray : public IDestroyComponentData {
			T* data;
			DestroyComponentDataArray(T* v) : data(v) {}
			void Destroy() override { delete[] data; }
		};

		extern std::vector<IDestroyComponentData*> queuedDataDestroy;
		extern std::vector<std::function<void()>> queuedActions;
	}

	template<typename T>
	static void QueueDestroyComponentData(T* v)
	{
		detail::queuedDataDestroy.push_back(new detail::DestroyComponentData<T>(v));
	}

	template<typename T>
	static void QueueDestroyComponentDataArray(T* v)
	{
		detail::queuedDataDestroy.push_back(new detail::DestroyComponentDataArray<T>(v));
	}

	void QueueComponentDataAction(std::function<void()>&& v);
	void DoQueuedComponentDataActions();
}