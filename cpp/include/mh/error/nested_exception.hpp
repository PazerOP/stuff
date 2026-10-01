#pragma once

#include <cstddef>
#include <exception>
#include <utility>

namespace mh
{
	namespace detail::nested_exception_hpp
	{
		template<bool bottomUp, typename TException, typename TFunc>
		inline void for_each_nested_exception_impl(const TException& e, TFunc&& func, size_t depth = 0)
		{
			try
			{
				std::rethrow_if_nested(e);
			}
			catch (const std::exception& e)
			{
				if constexpr (bottomUp)
					for_each_nested_exception_impl<bottomUp>(e, std::forward<TFunc>(func), depth + 1);

				func(e, depth);

				if constexpr (!bottomUp)
					for_each_nested_exception_impl<bottomUp>(e, std::forward<TFunc>(func), depth + 1);
			}
			catch (...)
			{
				// Exception does not inherit from std::exception; it cannot be
				// inspected for further nesting, so stop here.
			}
		}
	}

	template<bool bottomUp = false, typename TException, typename TFunc>
	inline void for_each_nested_exception(const TException& e, TFunc&& func)
	{
		return detail::nested_exception_hpp::for_each_nested_exception_impl<bottomUp>(e, std::forward<TFunc>(func));
	}
}
