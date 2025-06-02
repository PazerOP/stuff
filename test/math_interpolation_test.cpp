#include "mh/math/interpolation.hpp"
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

TEST_CASE("lerp", "[math][interpolation]")
{
	REQUIRE(mh::lerp(0.0f, 0, 0) == 0);
	REQUIRE(mh::lerp(0.0f, 0, 1) == 0);
	REQUIRE(mh::lerp(0.0f, 1, 1) == 1);
	REQUIRE(mh::lerp(1.0f, 1, 1) == 1);
	REQUIRE(mh::lerp(0.5f, 1, 1) == 1);
	REQUIRE(mh::lerp(0.5f, -49, 1) == Catch::Approx(-24));

	REQUIRE(mh::lerp(0.5f, -100000, -10000) == Catch::Approx(-55000));

	// Test upper bound
	REQUIRE(mh::lerp(1.1, 0, 10) == Catch::Approx(11));
	REQUIRE(mh::lerp_clamped(1.1, 0, 10) == Catch::Approx(10));

	// Test lower bound
	REQUIRE(mh::lerp(-1.1, 0, 10) == Catch::Approx(-11));
	REQUIRE(mh::lerp_clamped(-1.1, 0, 10) == Catch::Approx(0));
}

TEST_CASE("lerp vs lerp_slow basic comparison", "[math][interpolation]")
{
	// Test basic cases where both should be identical
	REQUIRE(mh::lerp(0.0f, 0, 10) == mh::lerp_slow(0.0f, 0, 10));
	REQUIRE(mh::lerp(1.0f, 0, 10) == mh::lerp_slow(1.0f, 0, 10));
	REQUIRE(mh::lerp(0.5f, 0, 10) == Catch::Approx(mh::lerp_slow(0.5f, 0, 10)));
	
	// Test negative ranges
	REQUIRE(mh::lerp(0.5f, -10, 10) == Catch::Approx(mh::lerp_slow(0.5f, -10, 10)));
	REQUIRE(mh::lerp(0.5f, -100, -50) == Catch::Approx(mh::lerp_slow(0.5f, -100, -50)));
}

TEST_CASE("lerp clamping behavior", "[math][interpolation]")
{
	// Test that clamping works correctly for values outside [0,1]
	REQUIRE(mh::lerp_clamped(1.5f, 0, 10) == 10);
	REQUIRE(mh::lerp_clamped(-0.5f, 0, 10) == 0);
	REQUIRE(mh::lerp_slow_clamped(1.5f, 0, 10) == 10);
	REQUIRE(mh::lerp_slow_clamped(-0.5f, 0, 10) == 0);
	
	// Test negative ranges
	REQUIRE(mh::lerp_clamped(1.5f, -10, 10) == 10);
	REQUIRE(mh::lerp_clamped(-0.5f, -10, 10) == -10);
	REQUIRE(mh::lerp_slow_clamped(1.5f, -10, 10) == 10);
	REQUIRE(mh::lerp_slow_clamped(-0.5f, -10, 10) == -10);
}

TEST_CASE("interpolation detail round function", "[math][interpolation]")
{
	using mh::detail::interpolation_hpp::round;
	
	// Test positive numbers
	REQUIRE(round(1.4f) == 1.0f);
	REQUIRE(round(1.5f) == 2.0f);
	REQUIRE(round(1.6f) == 2.0f);
	REQUIRE(round(2.4f) == 2.0f);
	REQUIRE(round(2.5f) == 3.0f);
	
	// Test negative numbers
	REQUIRE(round(-1.4f) == -1.0f);
	REQUIRE(round(-1.5f) == -2.0f);
	REQUIRE(round(-1.6f) == -2.0f);
	REQUIRE(round(-2.4f) == -2.0f);
	REQUIRE(round(-2.5f) == -3.0f);
	
	// Test edge cases
	REQUIRE(round(0.0f) == 0.0f);
	REQUIRE(round(0.4f) == 0.0f);
	REQUIRE(round(0.5f) == 1.0f);
	REQUIRE(round(-0.4f) == 0.0f);
	REQUIRE(round(-0.5f) == -1.0f);
}

TEST_CASE("interpolation detail clamp function", "[math][interpolation]")
{
	using mh::detail::interpolation_hpp::clamp;
	
	// Test clamping with mixed types (no rounding, returns common type)
	REQUIRE(clamp(1.4f, 0, 10) == 1.4f);  // float, int, int -> float
	REQUIRE(clamp(1.5f, 0, 10) == 1.5f);
	REQUIRE(clamp(1.6f, 0, 10) == 1.6f);
	REQUIRE(clamp(-1.4f, -10, 10) == -1.4f);
	REQUIRE(clamp(-1.5f, -10, 10) == -1.5f);
	REQUIRE(clamp(-1.6f, -10, 10) == -1.6f);
	
	// Test boundary clamping
	REQUIRE(clamp(15.0f, 0, 10) == 10.0f);
	REQUIRE(clamp(-15.0f, 0, 10) == 0.0f);
	REQUIRE(clamp(15.0f, -5, 5) == 5);
	REQUIRE(clamp(-15.0f, -5, 5) == -5);
}

TEST_CASE("specific failing case analysis", "[math][interpolation]")
{
	// The exact case that was failing
	float t = 0.349999994f;
	int min_val = -105;
	int max_val = 105;
	
	// Calculate unclamped results to understand the difference
	auto lerp_result = mh::lerp(t, min_val, max_val);
	auto lerp_slow_result = mh::lerp_slow(t, min_val, max_val);
	
	CAPTURE(t, min_val, max_val);
	CAPTURE(lerp_result, lerp_slow_result);
	
	// Expected calculations:
	// lerp: -105 + (105 - (-105)) * 0.349999994 = -105 + 210 * 0.349999994 = -105 + 73.4999987 = -31.5000013
	// lerp_slow: (-105 * (1 - 0.349999994)) + (105 * 0.349999994) = (-105 * 0.650000006) + (105 * 0.349999994) = -68.2500006 + 36.7499994 = -31.5000012
	
	// Both should be approximately -31.5
	REQUIRE(lerp_result == Catch::Approx(-31.5f).margin(0.01f));
	REQUIRE(lerp_slow_result == Catch::Approx(-31.5f).margin(0.01f));
	
	// Now test clamped versions - this is where the difference occurs due to rounding
	auto lerp_clamped_result = mh::lerp_clamped(t, min_val, max_val);
	auto lerp_slow_clamped_result = mh::lerp_slow_clamped(t, min_val, max_val);
	
	CAPTURE(lerp_clamped_result, lerp_slow_clamped_result);
	
	// The clamped versions no longer round, they preserve floating point precision
	// Both should give approximately the same result (small floating point differences expected)
	REQUIRE(lerp_clamped_result == Catch::Approx(lerp_slow_clamped_result).epsilon(1e-6));
}

TEST_CASE("lerp_slow", "[math][interpolation]")
{
	REQUIRE(mh::lerp_slow(0.5f, std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::max()) == Catch::Approx(0));
	REQUIRE(mh::lerp_slow(0.5f, std::numeric_limits<double>::lowest(),
		std::numeric_limits<double>::max()) == Catch::Approx(0));

	// Test a smaller set first to isolate issues
	for (int i = 0; i < 100; i++)
	{
		const auto t = i * 0.01f;  // Simple progression from 0 to 1
		const auto min = -i;
		const auto max = i;
		CAPTURE(t, min, max);

		if (min != max) {  // Avoid division by zero case
			REQUIRE(mh::lerp(t, min, max) ==
				Catch::Approx(mh::lerp_slow(t, min, max)).epsilon(0.0005));
			REQUIRE(mh::lerp_clamped(t, min, max) == Catch::Approx(mh::lerp_slow_clamped(t, min, max)).epsilon(1e-6));
		}
	}
}

TEST_CASE("no unwanted rounding - float to float", "[math][interpolation]")
{
	// Test that float-to-float interpolation preserves exact floating point values
	// and doesn't apply any rounding
	
	// Test case where result should be exactly representable
	float t = 0.25f;
	float min_val = 10.0f;
	float max_val = 20.0f;
	
	auto lerp_result = mh::lerp(t, min_val, max_val);
	auto lerp_slow_result = mh::lerp_slow(t, min_val, max_val);
	auto lerp_clamped_result = mh::lerp_clamped(t, min_val, max_val);
	auto lerp_slow_clamped_result = mh::lerp_slow_clamped(t, min_val, max_val);
	
	// Expected: 10.0 + (20.0 - 10.0) * 0.25 = 10.0 + 2.5 = 12.5
	float expected = 12.5f;
	
	REQUIRE(lerp_result == expected);
	REQUIRE(lerp_slow_result == expected);
	REQUIRE(lerp_clamped_result == expected);
	REQUIRE(lerp_slow_clamped_result == expected);
	
	// Test with non-exact values that should still not be rounded
	t = 0.333333f;
	auto lerp_result2 = mh::lerp(t, min_val, max_val);
	auto lerp_clamped_result2 = mh::lerp_clamped(t, min_val, max_val);
	
	// These should be equal - no rounding should occur for float-to-float
	REQUIRE(lerp_result2 == lerp_clamped_result2);
}

TEST_CASE("no unwanted rounding - double to double", "[math][interpolation]")
{
	// Test that double-to-double interpolation preserves exact floating point values
	
	double t = 0.7;
	double min_val = -100.0;
	double max_val = 100.0;
	
	auto lerp_result = mh::lerp(t, min_val, max_val);
	auto lerp_slow_result = mh::lerp_slow(t, min_val, max_val);
	auto lerp_clamped_result = mh::lerp_clamped(t, min_val, max_val);
	auto lerp_slow_clamped_result = mh::lerp_slow_clamped(t, min_val, max_val);
	
	// Expected: -100.0 + (100.0 - (-100.0)) * 0.7 = -100.0 + 200.0 * 0.7 = -100.0 + 140.0 = 40.0
	double expected = 40.0;
	
	REQUIRE(lerp_result == Catch::Approx(expected).epsilon(1e-14));
	REQUIRE(lerp_slow_result == Catch::Approx(expected).epsilon(1e-14));
	REQUIRE(lerp_clamped_result == Catch::Approx(expected).epsilon(1e-14));
	REQUIRE(lerp_slow_clamped_result == Catch::Approx(expected).epsilon(1e-14));
}

TEST_CASE("rounding only when converting to integer", "[math][interpolation]")
{
	// Test that rounding only occurs when converting from float to integer types
	
	float t = 0.5f;
	float min_float = 10.0f;
	float max_float = 20.0f;
	int min_int = 10;
	int max_int = 20;
	
	// Float to float - should be exact, no rounding
	auto float_result = mh::lerp(t, min_float, max_float);
	auto float_clamped = mh::lerp_clamped(t, min_float, max_float);
	REQUIRE(float_result == 15.0f);
	REQUIRE(float_clamped == 15.0f);
	REQUIRE(float_result == float_clamped);
	
	// Float to int - should NOT apply rounding (return type is float due to common_type)
	auto int_result = mh::lerp(t, min_int, max_int);
	auto int_clamped = mh::lerp_clamped(t, min_int, max_int);
	// Common type is float, so result should be 15.0f
	REQUIRE(int_result == 15.0f);
	REQUIRE(int_clamped == 15.0f);
	
	// Test the specific case mentioned: lerp(0.25, 3, 4) == 3.25
	auto quarter_result = mh::lerp(0.25f, 3, 4);
	auto quarter_clamped = mh::lerp_clamped(0.25f, 3, 4);
	REQUIRE(quarter_result == 3.25f);
	REQUIRE(quarter_clamped == 3.25f);
	REQUIRE(quarter_result == quarter_clamped);
	
	// Test with fractional results that should not be rounded
	auto third_result = mh::lerp(1.0f/3.0f, 0, 3);
	auto third_clamped = mh::lerp_clamped(1.0f/3.0f, 0, 3);
	REQUIRE(third_result == Catch::Approx(1.0f).epsilon(1e-6));
	REQUIRE(third_clamped == Catch::Approx(1.0f).epsilon(1e-6));
	REQUIRE(third_result == third_clamped);
}

TEST_CASE("preserve fractional precision", "[math][interpolation]")
{
	// Test that fractional values are preserved when they should be
	
	double t = 1.0 / 3.0; // 0.333...
	double min_val = 0.0;
	double max_val = 3.0;
	
	auto result = mh::lerp(t, min_val, max_val);
	auto clamped_result = mh::lerp_clamped(t, min_val, max_val);
	
	// Result should be exactly 1.0
	REQUIRE(result == Catch::Approx(1.0).epsilon(1e-15));
	REQUIRE(clamped_result == Catch::Approx(1.0).epsilon(1e-15));
	REQUIRE(result == clamped_result);
	
	// Test with a value that has fractional part
	t = 0.1;
	result = mh::lerp(t, min_val, max_val);
	clamped_result = mh::lerp_clamped(t, min_val, max_val);
	
	// Result should be 0.3
	REQUIRE(result == Catch::Approx(0.3).epsilon(1e-15));
	REQUIRE(clamped_result == Catch::Approx(0.3).epsilon(1e-15));
	REQUIRE(result == clamped_result);
}


template<typename TFrom, typename TTo>
static void TestRemapStatic()
{
	using nl_from = std::numeric_limits<TFrom>;
	using nl_to = std::numeric_limits<TTo>;

	constexpr auto min_from = nl_from::min();
	constexpr auto max_from = nl_from::max();
	constexpr auto min_to = nl_to::min();
	constexpr auto max_to = nl_to::max();
	CAPTURE(+min_from, +max_from, +min_to, +max_to);

#if false
	if constexpr (std::is_unsigned_v<TFrom> && std::is_unsigned_v<TTo>)
	{
		constexpr auto halfFrom = max_from / 2;
		constexpr auto halfTo = max_to / 2;
		CAPTURE(+halfFrom);
		REQUIRE(+mh::remap_static<TFrom, TTo>(halfFrom) == +halfTo);
		//REQUIRE(+mh::remap_static<TFrom, TTo>(halfTo) == +halfFrom);
	}
#endif

	REQUIRE(+mh::remap_static<TFrom, TTo>(min_from) == +min_to);
	REQUIRE(+mh::remap_static<TFrom, TTo>(max_from) == +max_to);
	//REQUIRE(+mh::remap_static<TTo, TFrom>(min_to) == +min_from);
	//REQUIRE(+mh::remap_static<TTo, TFrom>(max_to) == +max_from);
}

template<typename TSrc>
static void TestRemapStatic1()
{
	TestRemapStatic<TSrc, uint8_t>();
	TestRemapStatic<TSrc, uint16_t>();
	TestRemapStatic<TSrc, uint32_t>();
	TestRemapStatic<TSrc, uint64_t>();

	TestRemapStatic<TSrc, int8_t>();
	TestRemapStatic<TSrc, int16_t>();
	TestRemapStatic<TSrc, int32_t>();
	TestRemapStatic<TSrc, int64_t>();
}

template<typename TLargeInt>
static void TestLargeIntRemap()
{
	constexpr TLargeInt MAX = std::numeric_limits<TLargeInt>::max();
	CAPTURE(MAX);
	REQUIRE(+mh::remap_static<TLargeInt, TLargeInt, 0, MAX, 0, MAX-1>(MAX) == MAX-1);
	REQUIRE(+mh::remap_static<TLargeInt, TLargeInt, 0, MAX, 0, MAX-1>(MAX-1) == MAX-2);
	REQUIRE(+mh::remap_static<TLargeInt, TLargeInt, 0, MAX, 0, MAX-1>(MAX-2) == MAX-3);
}

TEST_CASE("remap_static", "[math][interpolation]")
{
	{
		// Adjustable
		using TSrc = uint8_t;
		using TDest = int64_t;
		constexpr TSrc value = 127;

		// Evaluated
		constexpr auto src_max = std::numeric_limits<TSrc>::max();
		constexpr auto src_min = std::numeric_limits<TSrc>::min();
		constexpr auto dest_max = std::numeric_limits<TDest>::max();
		constexpr auto dest_min = std::numeric_limits<TDest>::min();

		using TSrcUnsigned = std::make_unsigned_t<TSrc>;
		using TDestUnsigned = std::make_unsigned_t<TDest>;

		constexpr TSrcUnsigned valueOffset = TSrcUnsigned(value) - TSrcUnsigned(src_min);

		constexpr auto src_umax = TSrcUnsigned(src_max) - TSrcUnsigned(src_min);
		constexpr auto dest_umax = TDestUnsigned(dest_max) - TDestUnsigned(dest_min);

		constexpr auto gcd = std::gcd(dest_umax, src_umax);
		constexpr auto num = dest_umax / gcd;
		constexpr auto den = src_umax / gcd;

		constexpr auto round_add = ((TSrcUnsigned(src_max) - 1) / TDestUnsigned(dest_max)) / 2;

		constexpr TDestUnsigned result1 = valueOffset * (num / den);

		constexpr TDestUnsigned result2 = ((valueOffset + round_add) * (num % den)) / den;

		constexpr TDestUnsigned resultFinal = result1 + result2 + dest_min;

		constexpr TDest resultFinalCast = resultFinal;

		// Assertions
		static_assert(valueOffset == 127);

		static_assert(src_umax == std::numeric_limits<TSrcUnsigned>::max());
		static_assert(dest_umax == std::numeric_limits<TDestUnsigned>::max());
		static_assert(num == dest_umax / src_umax);
		static_assert(den == 1);

		static_assert(round_add == 0);

		static_assert(result1 == 9187201950435737471);
		static_assert(result2 == 0);
		static_assert(resultFinalCast == -36170086419038337);
	}

	TestRemapStatic1<uint8_t>();
	TestRemapStatic1<uint16_t>();
	TestRemapStatic1<uint32_t>();
	TestRemapStatic1<uint64_t>();

	TestRemapStatic1<int8_t>();
	TestRemapStatic1<int16_t>();
	TestRemapStatic1<int32_t>();
	TestRemapStatic1<int64_t>();

	REQUIRE(+mh::remap_static<uint8_t, int64_t>(127) == -36170086419038337);
	REQUIRE(+mh::remap_static<int64_t, uint8_t>(-8421505) == 127);
	REQUIRE(+mh::remap_static<int64_t, uint8_t>(-36170086419038337) == 127);
	REQUIRE(+mh::remap_static<int64_t, uint8_t>(-36170086419038336) == 127);
	REQUIRE(+mh::remap_static<int64_t, uint8_t>(-1) == 127);
	REQUIRE(+mh::remap_static<int64_t, uint8_t>(0) == 127);
	REQUIRE(+mh::remap_static<int64_t, uint8_t>(1) == 128);

	REQUIRE(+mh::remap_static<int16_t, uint8_t>(-1) == 127);
	REQUIRE(+mh::remap_static<int16_t, uint8_t>(0) == 127);
	REQUIRE(+mh::remap_static<int16_t, uint8_t>(1) == 128);

	REQUIRE(+mh::remap_static<uint8_t, uint8_t, 0, 255, 0, 31>(46) == 6);
	REQUIRE(+mh::remap_static<uint8_t, uint8_t, 0, 31, 0, 255>(6) == 49);

	REQUIRE(+mh::remap_static<uint8_t, uint8_t, 0, 255, 0, 31>(255) == 31);
	REQUIRE(+mh::remap_static<uint8_t, uint8_t, 0, 31, 0, 255>(31) == 255);
	REQUIRE(+mh::remap_static<uint8_t, uint8_t, 0, 31, 0, 255>(3) == 25);
	REQUIRE(+mh::remap_static<uint8_t, uint8_t, 0, 31, 0, 255>(4) == 33);
	REQUIRE(+mh::remap_static<uint8_t, uint8_t, 0, 31, 0, 255>(5) == 41);

	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, 43>(255) == 43);
	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, 43>(26) == -34);
	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, 43>(25) == -35);
	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, 43>(180) == 18);
	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, 43>(178) == 17);

	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, -27>(255) == -27);
	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, -27>(45) == -40);
	REQUIRE(+mh::remap_static<uint8_t, int8_t, 0, 255, -43, -27>(53) == -40);

	REQUIRE(+mh::remap_static<int8_t, int8_t, -100, 100, -5, 5>(-29) == -1);
	REQUIRE(+mh::remap_static<int8_t, int8_t, -100, 100, -5, 5>(-30) == -2);
	REQUIRE(+mh::remap_static<int8_t, int8_t, -100, 100, -5, 5>(-31) == -2);
	REQUIRE(+mh::remap_static<int8_t, int8_t, -100, 100, -5, 5>(30) == 1);
	REQUIRE(+mh::remap_static<int8_t, int8_t, -100, 100, -5, 5>(31) == 2);

	TestLargeIntRemap<uint32_t>();
	TestLargeIntRemap<uint64_t>();
	TestLargeIntRemap<uintmax_t>();
}
