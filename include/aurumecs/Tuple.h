#pragma once

#include <tuple>

namespace au {
	namespace detail {
		template <size_t> struct _sizeT {
		};

		template <typename TupleType, typename ActionType>
		inline void _foreach_tuple_element_impl(TupleType& tuple, ActionType action, _sizeT<0>)
		{
		}

		template <typename TupleType, typename ActionType, size_t N>
		inline void _foreach_tuple_element_impl(TupleType& tuple, ActionType action, _sizeT<N>)
		{
			_foreach_tuple_element_impl(tuple, action, _sizeT<N - 1>());
			action(std::get<N - 1>(tuple));
		}
	}

	template <typename TupleType, typename ActionType>
	inline void foreach_tuple_element(TupleType& tuple, ActionType action)
	{
		detail::_foreach_tuple_element_impl(tuple, action, detail::_sizeT<std::tuple_size<TupleType>::value>());
	}
}