#include "mh/algorithm/algorithm.hpp"
#include <catch2/catch_all.hpp>

#include <functional>
#include <string>
#include <vector>

TEST_CASE("find_or_add empty vector", "[algorithm]")
{
	std::vector<std::string> vec;

	SECTION("Empty vector find and add")
	{
		auto result = mh::find_or_add(vec, "hello");
		REQUIRE(result.first == true);
		REQUIRE(result.second == vec.begin());
		REQUIRE(*result.second == "hello");
		REQUIRE(vec.size() == 1);
		REQUIRE(vec.at(0) == "hello");
	}
}

TEST_CASE("find_or_add populated vector", "[algorithm]")
{
	std::vector<std::string> vec;
	vec.push_back("my cool string");
	vec.push_back("this one's pretty cool too");

	SECTION("Existing vector find")
	{
		auto result = mh::find_or_add(vec, "my cool string");
		REQUIRE(result.first == false);
		REQUIRE(vec.size() == 2);
		REQUIRE(*result.second == "my cool string");
	}
	SECTION("Existing vector add")
	{
		auto result = mh::find_or_add(vec, "my even cooler string");
		REQUIRE(result.first == true);
		REQUIRE(vec.size() == 3);
		REQUIRE(*result.second == "my even cooler string");
	}
}

TEST_CASE("contains", "[algorithm]")
{
	const std::vector<int> vec{ 1, 2, 3 };
	REQUIRE(mh::contains(vec, 1));
	REQUIRE(mh::contains(vec, 2));
	REQUIRE(mh::contains(vec, 3));
	REQUIRE(!mh::contains(vec, 4));

	const std::vector<int> empty;
	REQUIRE(!mh::contains(empty, 1));
}

TEST_CASE("erase", "[algorithm]")
{
	std::vector<int> vec{ 1, 2, 3, 2, 4 };

	mh::erase(vec, 2);
	REQUIRE((vec == std::vector<int>{ 1, 3, 4 }));

	// erasing an absent value is a no-op
	mh::erase(vec, 99);
	REQUIRE((vec == std::vector<int>{ 1, 3, 4 }));
}

TEST_CASE("sort", "[algorithm]")
{
	std::vector<int> vec{ 3, 1, 2 };
	mh::sort(vec);
	REQUIRE((vec == std::vector<int>{ 1, 2, 3 }));
}

namespace
{
	// Flags the moved-from state so we can prove mh::sort never moves from a
	// caller-owned comparator.
	struct comparator_probe
	{
		bool m_MovedFrom = false;

		comparator_probe() = default;
		comparator_probe(const comparator_probe&) = default;
		comparator_probe(comparator_probe&& other) noexcept
		{
			other.m_MovedFrom = true;
		}

		bool operator()(int lhs, int rhs) const { return lhs > rhs; }
	};
}

TEST_CASE("sort with comparator", "[algorithm]")
{
	std::vector<int> vec{ 1, 3, 2 };

	SECTION("lvalue comparator is not moved from")
	{
		comparator_probe compare;
		mh::sort(vec, compare);
		REQUIRE((vec == std::vector<int>{ 3, 2, 1 }));
		REQUIRE(!compare.m_MovedFrom);

		// a stateful lvalue comparator stays usable after the sort
		std::function<bool(int, int)> ascending = [](int lhs, int rhs) { return lhs < rhs; };
		mh::sort(vec, ascending);
		REQUIRE((vec == std::vector<int>{ 1, 2, 3 }));
		REQUIRE(static_cast<bool>(ascending));
		REQUIRE(ascending(1, 2));
	}
	SECTION("rvalue comparator")
	{
		mh::sort(vec, comparator_probe{});
		REQUIRE((vec == std::vector<int>{ 3, 2, 1 }));
	}
}
