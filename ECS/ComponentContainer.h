#pragma once

#include <vector>

namespace au {
	template<typename T>
	struct ComponentContainer {
		using value_type = T;

		std::vector<T> PresentBuffer;
		std::vector<T> FutureBuffer;
	};
}