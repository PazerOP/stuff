#include "mh/math/random.hpp"
#include <catch2/catch_all.hpp>

#include <set>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <thread>

TEST_CASE("get_random integer types", "[math][random]")
{
	SECTION("int32_t range")
	{
		constexpr int32_t min_val = 10;
		constexpr int32_t max_val = 20;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("uint32_t range")
	{
		constexpr uint32_t min_val = 100u;
		constexpr uint32_t max_val = 200u;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("int64_t range")
	{
		constexpr int64_t min_val = 1000LL;
		constexpr int64_t max_val = 2000LL;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("uint64_t range")
	{
		constexpr uint64_t min_val = 10000ULL;
		constexpr uint64_t max_val = 20000ULL;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("int16_t range")
	{
		constexpr int16_t min_val = 100;
		constexpr int16_t max_val = 200;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("uint16_t range")
	{
		constexpr uint16_t min_val = 300;
		constexpr uint16_t max_val = 400;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("int8_t range")
	{
		constexpr int8_t min_val = 10;
		constexpr int8_t max_val = 20;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("uint8_t range")
	{
		constexpr uint8_t min_val = 50;
		constexpr uint8_t max_val = 60;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}
}

TEST_CASE("get_random floating point types", "[math][random]")
{
	SECTION("float range")
	{
		constexpr float min_val = 1.0f;
		constexpr float max_val = 2.0f;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("double range")
	{
		constexpr double min_val = 10.5;
		constexpr double max_val = 20.5;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("float negative range")
	{
		constexpr float min_val = -5.0f;
		constexpr float max_val = -1.0f;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}

	SECTION("double zero-crossing range")
	{
		constexpr double min_val = -2.5;
		constexpr double max_val = 2.5;
		
		for (int i = 0; i < 100; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}
}

TEST_CASE("get_random edge cases", "[math][random]")
{
	SECTION("single value range")
	{
		constexpr int value = 42;
		for (int i = 0; i < 10; ++i) {
			auto result = mh::get_random(value, value);
			REQUIRE(result == value);
		}
	}

	SECTION("zero range for integers")
	{
		for (int i = 0; i < 10; ++i) {
			auto result = mh::get_random(0, 0);
			REQUIRE(result == 0);
		}
	}

	SECTION("zero range for floats")
	{
		constexpr float zero = 0.0f;
		for (int i = 0; i < 10; ++i) {
			auto result = mh::get_random(zero, zero);
			REQUIRE(result == Catch::Approx(zero));
		}
	}

	SECTION("maximum integer range")
	{
		// Test with smaller range to avoid overflow issues
		constexpr int16_t min_val = std::numeric_limits<int16_t>::min();
		constexpr int16_t max_val = std::numeric_limits<int16_t>::max();
		
		for (int i = 0; i < 50; ++i) {
			auto result = mh::get_random(min_val, max_val);
			REQUIRE(result >= min_val);
			REQUIRE(result <= max_val);
		}
	}
}

TEST_CASE("get_random distribution quality", "[math][random]")
{
	SECTION("integer distribution coverage")
	{
		constexpr int min_val = 1;
		constexpr int max_val = 10;
		std::unordered_set<int> seen_values;
		
		// Generate many values to see good coverage
		for (int i = 0; i < 1000; ++i) {
			auto result = mh::get_random(min_val, max_val);
			seen_values.insert(result);
		}
		
		// Should see most or all values in the range
		REQUIRE(seen_values.size() >= 7); // At least 70% coverage
		
		// Verify all seen values are in range
		for (auto value : seen_values) {
			REQUIRE(value >= min_val);
			REQUIRE(value <= max_val);
		}
	}

	SECTION("float distribution spread")
	{
		constexpr float min_val = 0.0f;
		constexpr float max_val = 1.0f;
		std::vector<float> values;
		
		for (int i = 0; i < 1000; ++i) {
			values.push_back(mh::get_random(min_val, max_val));
		}
		
		// Calculate mean and verify it's approximately in the middle
		float mean = std::accumulate(values.begin(), values.end(), 0.0f) / values.size();
		REQUIRE(mean == Catch::Approx(0.5f).margin(0.1f));
		
		// Verify we have good spread (min and max should be reasonably close to bounds)
		auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
		REQUIRE(*min_it < 0.2f); // Should get close to minimum
		REQUIRE(*max_it > 0.8f); // Should get close to maximum
	}
}

TEST_CASE("get_random thread safety", "[math][random]")
{
	SECTION("multiple threads don't interfere")
	{
		constexpr int num_threads = 4;
		constexpr int values_per_thread = 100;
		
		std::vector<std::thread> threads;
		std::vector<std::vector<int>> thread_results(num_threads);
		
		for (int t = 0; t < num_threads; ++t) {
			threads.emplace_back([&thread_results, t, values_per_thread]() {
				for (int i = 0; i < values_per_thread; ++i) {
					thread_results[t].push_back(mh::get_random(1, 1000));
				}
			});
		}
		
		for (auto& thread : threads) {
			thread.join();
		}
		
		// Verify all threads generated valid values
		for (int t = 0; t < num_threads; ++t) {
			REQUIRE(thread_results[t].size() == values_per_thread);
			for (auto value : thread_results[t]) {
				REQUIRE(value >= 1);
				REQUIRE(value <= 1000);
			}
		}
		
		// Verify threads generated different sequences (very high probability)
		bool sequences_differ = false;
		for (int t1 = 0; t1 < num_threads - 1; ++t1) {
			for (int t2 = t1 + 1; t2 < num_threads; ++t2) {
				if (thread_results[t1] != thread_results[t2]) {
					sequences_differ = true;
					break;
				}
			}
			if (sequences_differ) break;
		}
		REQUIRE(sequences_differ); // Extremely unlikely that all sequences are identical
	}
}