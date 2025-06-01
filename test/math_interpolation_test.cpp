#include "mh/math/interpolation.hpp"
#include <catch2/catch_all.hpp>

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

TEST_CASE("lerp_slow", "[math][interpolation]")
{
	REQUIRE(mh::lerp_slow(0.5f, std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::max()) == Catch::Approx(0));
	REQUIRE(mh::lerp_slow(0.5f, std::numeric_limits<double>::lowest(),
		std::numeric_limits<double>::max()) == Catch::Approx(0));
	//REQUIRE(mh::lerp_slow(0.5f, std::numeric_limits<long double>::lowest(),
	//	std::numeric_limits<long double>::max()) == Catch::Approx(0));

	for (int i = 0; i < 1000; i++)
	{
		const auto t = i * ((i % 2) * 2 - 1) * 0.01f * (1.0f / 3);
		const auto min = -i;
		const auto max = i;
		CAPTURE(t, min, max);

		REQUIRE(mh::lerp(t, min, max) ==
			Catch::Approx(mh::lerp_slow(t, min, max)).epsilon(0.0005));
		REQUIRE(mh::lerp_clamped(t, min, max) == Catch::Approx(mh::lerp_slow_clamped(t, min, max)));
	}
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
