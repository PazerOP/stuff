#include "mh/math/random.hpp"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <thread>

// get_random is seeded from std::random_device: only deterministic properties
// are asserted (range bounds, degenerate ranges, absence of crashes) — never
// specific values or distribution shape.

namespace
{
	template<typename T>
	void check_in_bounds(T min, T max)
	{
		for (int i = 0; i < 1000; i++)
		{
			const T value = mh::get_random<T>(min, max);
			CHECK(value >= min);
			CHECK(value <= max);
		}
	}
}

TEMPLATE_TEST_CASE("get_random - integer bounds", "[math][random]",
	int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t)
{
	check_in_bounds<TestType>(0, 9);
	check_in_bounds<TestType>(TestType(17), TestType(17)); // degenerate range

	if constexpr (std::is_signed_v<TestType>)
		check_in_bounds<TestType>(TestType(-50), TestType(-40));

	// full domain (for the 8-bit types this exercises the widen-to-16-bit
	// delegation with values a plain cast round-trip must preserve)
	check_in_bounds<TestType>(std::numeric_limits<TestType>::min(), std::numeric_limits<TestType>::max());
}

TEMPLATE_TEST_CASE("get_random - floating point bounds", "[math][random]", float, double)
{
	check_in_bounds<TestType>(TestType(0), TestType(1));
	check_in_bounds<TestType>(TestType(-2.5), TestType(2.5));

	// max_inclusive is nudged with nextafter, so a degenerate range's only
	// representable result is the bound itself
	for (int i = 0; i < 100; i++)
		CHECK(mh::get_random<TestType>(TestType(3.25), TestType(3.25)) == TestType(3.25));
}

TEST_CASE("get_random - engine is usable from any thread", "[math][random]")
{
	// the engine is thread_local and lazily seeded per thread; a fresh thread
	// must get its own working engine
	int value = -1;
	std::thread thread([](int& valueOut)
		{
			valueOut = mh::get_random<int>(0, 100);
		}, std::ref(value));
	thread.join();

	CHECK(value >= 0);
	CHECK(value <= 100);
}
