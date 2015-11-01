#pragma once

#include <type_traits>
#include <tuple>

namespace au {
	namespace detail {
		template<typename Type, typename PackType, typename... T>
		struct is_element_of_impl;

		template<typename Type, typename PackType>
		struct is_element_of_impl<Type, PackType> {
			using value = typename std::conditional<std::is_same<Type, PackType>::value, std::true_type, std::false_type>::type;
		};

		template<typename Type, typename PackType, typename... T>
		struct is_element_of_impl {
			using value = typename std::conditional<std::is_same<Type, PackType>::value, std::true_type, typename is_element_of_impl<Type, T...>::value>::type;
		};

		template<typename... T>
		struct type_tuple_impl {
			template<typename... U>
			struct is_subset_of {
				using value = std::false_type;
			};

			template<typename U>
			struct contains {
				using value = std::false_type;
			};

			template<typename U, size_t pos = 0>
			struct index_of {
				using value = std::false_type;
			};

			template <typename ActionType>
			static void for_each(ActionType& action, std::size_t pos_index = 0)
			{
			}
		};

		template<typename T>
		struct type_tuple_impl<T> {
			template<typename... U>
			struct is_subset_of {
				using value = typename is_element_of_impl<T, U...>::value;
			};

			template<typename U>
			struct contains {
				using value = typename std::is_same<T, U>;
			};

			template<typename U, size_t pos = 0>
			struct index_of {
				using value = typename std::conditional<std::is_same<T, U>::value, std::integral_constant<int, pos>, std::integral_constant<int, -1>>::type;
			};

			template <typename ActionType>
			static inline void for_each(ActionType& action, std::size_t pos_index = 0)
			{
				action((T*) nullptr, pos_index);
			}
		};

		template<typename T, typename... R>
		struct type_tuple_impl<T, R...> {
			template<typename... U>
			struct is_subset_of {
			private:
				static const auto _is_element = typename detail::is_element_of_impl<T, U...>::value::value;
			public:
				using value = typename std::conditional<_is_element, typename type_tuple_impl<R...>::template is_subset_of<U...>::value, std::false_type>::type;
			};

			template<typename U>
			struct contains {
				using value = typename std::conditional<std::is_same<T, U>::value, std::true_type, typename type_tuple_impl<R...>::template contains<U>::value>::type;
			};

			template<typename U, size_t pos = 0>
			struct index_of {
				using value = typename std::conditional<std::is_same<T, U>::value, std::integral_constant<int, pos>,
				typename type_tuple_impl<R...>::template index_of<U, pos + 1>::value>::type;
			};

			template <typename ActionType>
			static inline void for_each(ActionType& action, std::size_t pos_index = 0)
			{
				type_tuple_impl<T>::for_each(action, pos_index);
				type_tuple_impl<R...>::for_each(action, pos_index + 1);
			}
		};

		template<typename T>
		struct wrapper {
			typedef void type;
		};
	}

	template<typename Type, typename... T>
	struct is_element_of {
		using type = typename detail::is_element_of_impl<Type, T...>::value;
		static const auto value = type::value;
	};

	template<typename... T>
	struct type_seq {
		static const bool is_last = true;
		static const bool is_empty = true;
	};

	template<typename T>
	struct type_seq <T> {
		using head = T;
		using tail = void;

		static const bool is_last = true;
		static const bool is_empty = false;
	};

	template<typename T, typename... R>
	struct type_seq <T, R...> {
		using head = T;
		using tail = type_seq<R...>;

		static const bool is_last = false;
		static const bool is_empty = false;
	};

	template<typename... T>
	struct type_tuple {
		template<typename... U>
		struct is_subset_of {
			using type = typename detail::type_tuple_impl<T...>::template is_subset_of<U...>::value;
			static const auto value = type::value;
		};

		template<typename U>
		struct contains {
			using type = typename detail::type_tuple_impl<T...>::template contains<U>::value;
			static const auto value = type::value;
		};

		template<typename U>
		struct index_of {
			using type = typename detail::type_tuple_impl<T...>::template index_of<U>::value;
			static const auto value = type::value;
		};

		template <typename ActionType>
		static inline void for_each(ActionType& action)
		{
			detail::type_tuple_impl<T...>::for_each(action);
		}

		template<std::size_t index>
		using element_at = typename std::tuple_element<index, std::tuple<T...>>::type;

		using count_type_constant = std::integral_constant<std::size_t, sizeof...(T)>;
		using is_type_tuple = std::true_type;
		using as_tuple = std::tuple<T...>;
		using as_type_seq = type_seq<T...>;

		static const std::size_t count = sizeof...(T);
		static const std::size_t size = count;
	};

	template<typename T, typename = void>
	struct is_type_tuple {
		static const auto value = std::false_type::value;
	};

	template<typename T>
	struct is_type_tuple<T, typename detail::wrapper<typename T::is_type_tuple>::type> {
		static const auto value = std::true_type::value;
	};

	template<typename T, typename... U>
	struct index_of : type_tuple<U...>::template index_of<T>::type
	{
	};
}