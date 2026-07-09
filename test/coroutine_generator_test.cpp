#include "mh/coroutine/generator.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <catch2/catch_all.hpp>

#include <array>
#include <stdexcept>

namespace
{
	mh::generator<int> counting_generator(int count)
	{
		for (int i = 0; i < count; i++)
			co_yield i;
	}

	// Throws once `throwAfter` values have been yielded. throwAfter == 0 throws
	// before the first co_yield, anything larger throws mid-iteration.
	mh::generator<int> throwing_generator(int throwAfter)
	{
		for (int i = 0; ; i++)
		{
			if (i == throwAfter)
				throw std::runtime_error("generator failure");

			co_yield i;
		}
	}
}

TEST_CASE("generator - yields every value")
{
	int sum = 0;
	int count = 0;
	for (int v : counting_generator(4))
	{
		sum += v;
		count++;
	}

	REQUIRE(count == 4);
	REQUIRE(sum == 6);

	REQUIRE(counting_generator(5).count() == 5);
	REQUIRE(counting_generator(0).empty());
	REQUIRE(!counting_generator(1).empty());
}

TEST_CASE("generator - make_generator copies a range")
{
	const std::array<int, 3> values = { 10, 20, 30 };

	int sum = 0;
	for (int v : mh::make_generator(values.begin(), values.end()))
		sum += v;

	REQUIRE(sum == 60);
}

TEST_CASE("generator - exception before the first yield propagates")
{
	auto consume = []
	{
		for ([[maybe_unused]] int v : throwing_generator(0))
			FAIL("generator yielded a value even though it should have thrown immediately");
	};

	REQUIRE_THROWS_AS(consume(), std::runtime_error);
}

TEST_CASE("generator - exception after the first yield propagates")
{
	// regression: exceptions thrown by the generator body after the first co_yield
	// used to be swallowed by iterator::operator++ - the loop just ended silently
	// and the error vanished.
	int sum = 0;
	auto consume = [&sum]
	{
		for (int v : throwing_generator(2))
			sum += v;
	};

	REQUIRE_THROWS_AS(consume(), std::runtime_error);
	REQUIRE(sum == 1); // 0 and 1 were yielded before the throw
}

#endif
