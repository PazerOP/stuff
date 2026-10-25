#pragma once

#include <numbers>
#include <type_traits>

namespace mh
{
	template<typename T, typename TResult = std::common_type_t<T, float>>
	constexpr TResult deg2rad(T degrees)
	{
		return TResult(degrees) * TResult(std::numbers::pi / 180.0);
	}

	template<typename T, typename TResult = std::common_type_t<T, float>>
	constexpr TResult rad2deg(T radians)
	{
		return TResult(radians) * TResult(180.0 / std::numbers::pi);
	}
}
