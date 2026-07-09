#include "mh/containers/heap.hpp"
#include <catch2/catch_all.hpp>

#include <vector>

TEST_CASE("heap - initializer_list constructor establishes the heap invariant", "[containers][heap]")
{
	// The initializer_list constructor must produce a valid heap: front() is the
	// largest element and repeated pop() yields the values in descending order,
	// exactly as if every element had been push()ed individually.
	mh::heap<int> h({3, 1, 4, 1, 5, 9, 2, 6});
	REQUIRE(h.size() == 8);
	REQUIRE(h.front() == 9);

	const int expected_order[] = {9, 6, 5, 4, 3, 2, 1, 1};
	for (const int expected : expected_order)
	{
		CAPTURE(expected);
		REQUIRE(!h.empty());
		REQUIRE(h.front() == expected);
		h.pop();
	}

	REQUIRE(h.empty());
	REQUIRE(h.size() == 0);
}

TEST_CASE("heap - initializer_list constructor pops in the same order as a push-built heap", "[containers][heap]")
{
	const std::vector<int> values{42, -7, 0, 13, 42, 100, -50, 8, 8, 99};

	mh::heap<int> fromList({42, -7, 0, 13, 42, 100, -50, 8, 8, 99});

	mh::heap<int> fromPush;
	for (const int v : values)
		fromPush.push(v);

	REQUIRE(fromList.size() == fromPush.size());

	while (!fromPush.empty())
	{
		REQUIRE(!fromList.empty());
		CAPTURE(fromPush.front());
		REQUIRE(fromList.front() == fromPush.front());
		fromList.pop();
		fromPush.pop();
	}

	REQUIRE(fromList.empty());
}

TEST_CASE("heap - initializer_list constructor respects a custom comparator", "[containers][heap]")
{
	// A reversed comparator turns mh::heap into a min-heap; the constructor must
	// heapify with the user-provided comparator, not the default one.
	struct greater_than
	{
		bool operator()(int lhs, int rhs) const { return lhs > rhs; }
	};

	mh::heap<int, greater_than> h({3, 1, 4, 1, 5, 9, 2, 6});
	REQUIRE(h.front() == 1);

	const int expected_order[] = {1, 1, 2, 3, 4, 5, 6, 9};
	for (const int expected : expected_order)
	{
		CAPTURE(expected);
		REQUIRE(h.front() == expected);
		h.pop();
	}

	REQUIRE(h.empty());
}

TEST_CASE("heap - push/pop interleaved with initializer_list construction", "[containers][heap]")
{
	// Pushing into a list-constructed heap must keep the invariant intact.
	mh::heap<int> h({10, 30, 20});
	REQUIRE(h.front() == 30);

	h.push(25);
	REQUIRE(h.front() == 30);

	h.push(99);
	REQUIRE(h.front() == 99);

	const int expected_order[] = {99, 30, 25, 20, 10};
	for (const int expected : expected_order)
	{
		CAPTURE(expected);
		REQUIRE(h.front() == expected);
		h.pop();
	}

	REQUIRE(h.empty());
}
