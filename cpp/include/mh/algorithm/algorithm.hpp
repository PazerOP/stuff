#pragma once

#include <algorithm>

namespace mh
{
	template<typename Container, typename Predicate, typename Factory>
	std::pair<bool, typename Container::iterator> find_or_add_if_factory(Container& container, const Predicate& pred, const Factory& factory)
	{
		if (auto found = std::find_if(container.begin(), container.end(), pred); found != container.end())
			return { false, found };

		return { true, container.insert(container.end(), factory()) };
	}

	template<typename Container, typename Predicate, typename T>
	inline std::pair<bool, typename Container::iterator> find_or_add_if(Container& container, const Predicate& pred, const T& value)
	{
		return find_or_add_if_factory(container, pred, [&value]() { return value; });
	}

	template<typename Container, typename T>
	inline std::pair<bool, typename Container::iterator> find_or_add(Container& container, const T& value)
	{
		return find_or_add_if(container, [&value](const auto& v) { return v == value; }, value);
	}
}
