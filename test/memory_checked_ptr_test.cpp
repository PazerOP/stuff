#include "mh/memory/checked_ptr.hpp"
#include <catch2/catch_all.hpp>

#include <compare>

TEST_CASE("checked_ptr - has_value/get basics", "[memory][checked_ptr]")
{
	int value = 5;
	mh::checked_ptr<int> ptr(&value);
	REQUIRE(ptr.has_value());
	REQUIRE(ptr.get() == &value);
	REQUIRE(*ptr.get() == 5);

	mh::checked_ptr<int> null;
	REQUIRE(!null.has_value());
}

TEST_CASE("checked_ptr - equality comparisons", "[memory][checked_ptr]")
{
	int values[2] = { 1, 2 };
	const mh::checked_ptr<int> first(&values[0]);
	const mh::checked_ptr<int> alsoFirst(&values[0]);
	const mh::checked_ptr<int> second(&values[1]);
	const mh::checked_ptr<int> null;

	SECTION("checked_ptr vs checked_ptr")
	{
		REQUIRE(first == alsoFirst);
		REQUIRE(first != second);
		REQUIRE(null == mh::checked_ptr<int>{});
	}
	SECTION("raw pointer vs checked_ptr, both directions")
	{
		const int* raw = &values[0];
		REQUIRE(raw == first);
		REQUIRE(first == raw);
		REQUIRE(raw != second);
		REQUIRE(second != raw);
		REQUIRE(null == nullptr);
		REQUIRE(nullptr == null);
		REQUIRE_FALSE(first == nullptr);
	}
}

TEST_CASE("checked_ptr - three-way comparisons", "[memory][checked_ptr]")
{
	int values[2] = { 1, 2 };
	const mh::checked_ptr<int> first(&values[0]);
	const mh::checked_ptr<int> alsoFirst(&values[0]);
	const mh::checked_ptr<int> second(&values[1]);

	SECTION("checked_ptr vs checked_ptr")
	{
		REQUIRE(((first <=> alsoFirst) == std::strong_ordering::equal));
		REQUIRE(((first <=> second) == std::strong_ordering::less));
		REQUIRE(((second <=> first) == std::strong_ordering::greater));
	}
	SECTION("raw pointer vs checked_ptr, both directions")
	{
		const int* raw = &values[0];
		REQUIRE(((raw <=> first) == std::strong_ordering::equal));
		REQUIRE(((first <=> raw) == std::strong_ordering::equal));
		REQUIRE(((raw <=> second) == std::strong_ordering::less));
		REQUIRE(((second <=> raw) == std::strong_ordering::greater));
	}
}
