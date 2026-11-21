#include "mh/algorithm/multi_compare.hpp"

#include <catch2/catch_all.hpp>

#include <string_view>

TEST_CASE("any/none/all_compare with a custom comparison", "[algorithm][multi_compare]")
{
	const auto sameLength = [](std::string_view lhs, std::string_view rhs)
	{
		return lhs.size() == rhs.size();
	};

	CHECK(mh::any_compare(sameLength, "ab", "x", "yz"));
	CHECK_FALSE(mh::any_compare(sameLength, "ab", "x", "xyz"));
	CHECK(mh::none_compare(sameLength, "ab", "x", "xyz"));
	CHECK_FALSE(mh::none_compare(sameLength, "ab", "xy"));
	CHECK(mh::all_compare(sameLength, "ab", "xy", "12"));
	CHECK_FALSE(mh::all_compare(sameLength, "ab", "xy", "123"));
}

TEST_CASE("generated multi-compare helpers", "[algorithm][multi_compare]")
{
	// lhs is compared against EVERY rhs
	CHECK(mh::any_eq(5, 1, 5, 9));
	CHECK_FALSE(mh::any_eq(5, 1, 2, 3));
	CHECK(mh::none_eq(5, 1, 2, 3));
	CHECK_FALSE(mh::none_eq(5, 5));
	CHECK(mh::all_eq(5, 5, 5));
	CHECK_FALSE(mh::all_eq(5, 5, 6));

	CHECK(mh::any_neq(5, 5, 6));
	CHECK(mh::none_neq(5, 5, 5));
	CHECK(mh::all_neq(5, 1, 2, 3));

	CHECK(mh::any_greater(5, 9, 4));
	CHECK(mh::none_greater(5, 5, 6));
	CHECK(mh::all_greater(5, 1, 2));

	CHECK(mh::any_greater_equal(5, 5, 9));
	CHECK(mh::none_greater_equal(5, 6, 7));
	CHECK(mh::all_greater_equal(5, 5, 4));

	CHECK(mh::any_less(5, 6, 1));
	CHECK(mh::none_less(5, 5, 4));
	CHECK(mh::all_less(5, 6, 7));

	CHECK(mh::any_less_equal(5, 5, 1));
	CHECK(mh::none_less_equal(5, 4, 3));
	CHECK(mh::all_less_equal(5, 5, 6));

	// usable in constant expressions
	STATIC_CHECK(mh::any_eq(1, 3, 2, 1));
	STATIC_CHECK(mh::all_less(0, 1, 2, 3));
	STATIC_CHECK(mh::none_greater(0, 1, 2, 3));
}
