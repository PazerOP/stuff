#include "mh/containers/heap.hpp"
#include <catch2/catch_all.hpp>

#include <functional>
#include <string>

TEST_CASE("heap basic construction", "[containers][heap]")
{
	SECTION("default construction")
	{
		mh::heap<int> h;
		REQUIRE(h.empty());
		REQUIRE(h.size() == 0);
	}

	SECTION("construction with initializer list")
	{
		mh::heap<int> h{3, 1, 4, 1, 5};
		REQUIRE(!h.empty());
		REQUIRE(h.size() == 5);
		REQUIRE(h.front() == 5); // Max element should be at front
	}

	SECTION("construction with custom comparator")
	{
		// Min heap using greater<int>
		mh::heap<int, std::greater<int>> h(std::greater<int>{});
		h.push(3);
		h.push(1);
		h.push(4);
		REQUIRE(h.front() == 1); // Min element should be at front
	}

	SECTION("construction with initializer list and custom comparator")
	{
		// Min heap
		mh::heap<int, std::greater<int>> h({3, 1, 4, 1, 5}, std::greater<int>{});
		REQUIRE(h.front() == 1); // Min element should be at front
	}
}

TEST_CASE("heap push operations", "[containers][heap]")
{
	SECTION("push maintains heap property")
	{
		mh::heap<int> h;
		
		h.push(3);
		REQUIRE(h.front() == 3);
		REQUIRE(h.size() == 1);
		
		h.push(1);
		REQUIRE(h.front() == 3); // 3 > 1, so 3 should still be at front
		REQUIRE(h.size() == 2);
		
		h.push(4);
		REQUIRE(h.front() == 4); // 4 is now the maximum
		REQUIRE(h.size() == 3);
		
		h.push(2);
		REQUIRE(h.front() == 4); // 4 should still be the maximum
		REQUIRE(h.size() == 4);
	}

	SECTION("push rvalue")
	{
		mh::heap<std::string> h;
		
		std::string str = "hello";
		h.push(std::move(str));
		REQUIRE(h.front() == "hello");
		REQUIRE(str.empty()); // Should be moved from
		
		h.push(std::string("world"));
		// "world" > "hello" lexicographically
		REQUIRE(h.front() == "world");
	}
}

TEST_CASE("heap pop operations", "[containers][heap]")
{
	SECTION("pop maintains heap property")
	{
		mh::heap<int> h{3, 1, 4, 1, 5, 9, 2, 6};
		
		// Should pop elements in descending order for max heap
		REQUIRE(h.front() == 9);
		h.pop();
		REQUIRE(h.size() == 7);
		
		REQUIRE(h.front() == 6);
		h.pop();
		REQUIRE(h.size() == 6);
		
		REQUIRE(h.front() == 5);
		h.pop();
		REQUIRE(h.size() == 5);
		
		REQUIRE(h.front() == 4);
		h.pop();
		REQUIRE(h.size() == 4);
		
		REQUIRE(h.front() == 3);
		h.pop();
		REQUIRE(h.size() == 3);
		
		REQUIRE(h.front() == 2);
		h.pop();
		REQUIRE(h.size() == 2);
		
		// Now we have two 1s left, order might vary
		REQUIRE(h.front() == 1);
		h.pop();
		REQUIRE(h.size() == 1);
		
		REQUIRE(h.front() == 1);
		h.pop();
		REQUIRE(h.empty());
		REQUIRE(h.size() == 0);
	}
}

TEST_CASE("heap front access", "[containers][heap]")
{
	SECTION("const front access")
	{
		const mh::heap<int> h{3, 1, 4, 1, 5};
		REQUIRE(h.front() == 5);
	}

	SECTION("mutable front access")
	{
		mh::heap<int> h{3, 1, 4, 1, 5};
		REQUIRE(h.front() == 5);
		
		// We can modify the front element, but this breaks heap property
		// This is just testing that the reference is mutable
		int& front_ref = h.front();
		REQUIRE(&front_ref == &h.front());
	}
}

TEST_CASE("heap size and empty", "[containers][heap]")
{
	SECTION("empty heap")
	{
		mh::heap<int> h;
		REQUIRE(h.empty());
		REQUIRE(h.size() == 0);
	}

	SECTION("non-empty heap")
	{
		mh::heap<int> h;
		h.push(42);
		REQUIRE(!h.empty());
		REQUIRE(h.size() == 1);
		
		h.push(100);
		REQUIRE(!h.empty());
		REQUIRE(h.size() == 2);
		
		h.pop();
		REQUIRE(!h.empty());
		REQUIRE(h.size() == 1);
		
		h.pop();
		REQUIRE(h.empty());
		REQUIRE(h.size() == 0);
	}
}

TEST_CASE("heap equality comparison", "[containers][heap]")
{
	SECTION("equal heaps")
	{
		mh::heap<int> h1{1, 2, 3};
		mh::heap<int> h2{1, 2, 3};
		REQUIRE(h1 == h2);
	}

	SECTION("different heaps")
	{
		mh::heap<int> h1{1, 2, 3};
		mh::heap<int> h2{1, 2, 4};
		REQUIRE(!(h1 == h2));
	}

	SECTION("same elements different order")
	{
		mh::heap<int> h1;
		h1.push(1);
		h1.push(2);
		h1.push(3);
		
		mh::heap<int> h2;
		h2.push(3);
		h2.push(1);
		h2.push(2);
		
		// Even though elements were added in different order,
		// the underlying containers should be equivalent for heaps
		// with the same elements (this depends on implementation details)
		REQUIRE(h1 == h2);
	}

	SECTION("different sizes")
	{
		mh::heap<int> h1{1, 2, 3};
		mh::heap<int> h2{1, 2};
		REQUIRE(!(h1 == h2));
	}
}

TEST_CASE("heap with custom comparator", "[containers][heap]")
{
	SECTION("min heap behavior")
	{
		mh::heap<int, std::greater<int>> min_heap;
		min_heap.push(3);
		min_heap.push(1);
		min_heap.push(4);
		min_heap.push(1);
		min_heap.push(5);
		
		// Should pop elements in ascending order for min heap
		REQUIRE(min_heap.front() == 1);
		min_heap.pop();
		
		REQUIRE(min_heap.front() == 1);
		min_heap.pop();
		
		REQUIRE(min_heap.front() == 3);
		min_heap.pop();
		
		REQUIRE(min_heap.front() == 4);
		min_heap.pop();
		
		REQUIRE(min_heap.front() == 5);
		min_heap.pop();
		
		REQUIRE(min_heap.empty());
	}

	SECTION("custom comparator for strings")
	{
		// Heap ordered by string length (shorter strings have higher priority)
		auto length_comparator = [](const std::string& a, const std::string& b) {
			return a.length() > b.length(); // Reverse comparison for min-heap by length
		};
		
		mh::heap<std::string, decltype(length_comparator)> h(length_comparator);
		h.push("hello");
		h.push("hi");
		h.push("world");
		h.push("a");
		
		REQUIRE(h.front() == "a"); // Shortest string
		h.pop();
		
		REQUIRE(h.front() == "hi"); // Next shortest
		h.pop();
		
		// "hello" and "world" both have length 5, either could be next
		std::string next = h.front();
		REQUIRE((next == "hello" || next == "world"));
		REQUIRE(next.length() == 5);
	}
}

TEST_CASE("heap stress test", "[containers][heap]")
{
	SECTION("large number of operations")
	{
		mh::heap<int> h;
		
		// Push many elements
		for (int i = 0; i < 1000; ++i) {
			h.push(i);
		}
		
		REQUIRE(h.size() == 1000);
		REQUIRE(h.front() == 999); // Maximum element
		
		// Pop half the elements
		for (int i = 0; i < 500; ++i) {
			int expected = 999 - i;
			REQUIRE(h.front() == expected);
			h.pop();
		}
		
		REQUIRE(h.size() == 500);
		REQUIRE(h.front() == 499);
		
		// Add more elements
		for (int i = 1000; i < 1100; ++i) {
			h.push(i);
		}
		
		REQUIRE(h.size() == 600);
		REQUIRE(h.front() == 1099); // New maximum
	}
}