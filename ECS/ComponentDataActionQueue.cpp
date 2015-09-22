#pragma once

#include "ComponentDataActionQueue.h"

namespace au {
	namespace detail {
		std::vector<IDestroyComponentData*> queuedDataDestroy;
		std::vector<std::function<void()>> queuedActions;
	}

	void QueueComponentDataAction(std::function<void()>&& v)
	{
		detail::queuedActions.push_back(std::move(v));
	}

	void DoQueuedComponentDataActions()
	{
		for (auto& v : detail::queuedDataDestroy)
		{
			v->Destroy();
			delete v;
		}

		detail::queuedDataDestroy.clear();

		for (auto& v : detail::queuedActions)
			v();

		detail::queuedActions.clear();
	}

}