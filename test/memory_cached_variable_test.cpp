#include "mh/memory/cached_variable.hpp"
#include <catch2/catch_all.hpp>

#include <chrono>

namespace
{
	// A manually-advanced clock so the caching behavior can be tested
	// deterministically (no sleeps, no wall-clock dependence).
	struct fake_clock
	{
		using rep = long long;
		using period = std::milli;
		using duration = std::chrono::duration<rep, period>;
		using time_point = std::chrono::time_point<fake_clock, duration>;
		[[maybe_unused]] static constexpr bool is_steady = true;

		inline static time_point s_Now{};
		static time_point now() { return s_Now; }
	};

	int s_UpdateCalls = 0;
	int countingUpdate()
	{
		return ++s_UpdateCalls;
	}
}

TEMPLATE_TEST_CASE_SIG("cached_variable - caches until the update interval elapses",
	"[memory][cached_variable]", ((bool ThreadSafe), ThreadSafe), true, false)
{
	using namespace std::chrono_literals;

	fake_clock::s_Now = fake_clock::time_point{}; // t = 0ms
	s_UpdateCalls = 0;

	mh::cached_variable<ThreadSafe, int, int (*)(), fake_clock> cached(10ms, &countingUpdate);

	// the initial value is computed at construction
	REQUIRE(s_UpdateCalls == 1);
	REQUIRE(cached.get_no_update() == 1);

	// within the interval, get() serves the cached value
	REQUIRE(cached.get() == 1);
	REQUIRE(s_UpdateCalls == 1);
	REQUIRE(cached.time_since_update() == 0ms);
	REQUIRE(cached.time_until_update() == 10ms);

	fake_clock::s_Now += 4ms;
	REQUIRE(cached.get() == 1);
	REQUIRE(s_UpdateCalls == 1);
	REQUIRE(cached.time_since_update() == 4ms);
	REQUIRE(cached.time_until_update() == 6ms);

	// get_no_update() never refreshes, even past the deadline
	fake_clock::s_Now += 100ms;
	REQUIRE(cached.get_no_update() == 1);
	REQUIRE(s_UpdateCalls == 1);

	// once the interval has elapsed, get() refreshes and re-arms the timer
	REQUIRE(cached.get() == 2);
	REQUIRE(s_UpdateCalls == 2);
	REQUIRE(cached.time_since_update() == 0ms);
	REQUIRE(cached.time_until_update() == 10ms);
}
