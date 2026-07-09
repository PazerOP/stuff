#include "mh/math/uint128.hpp"
#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <sstream>

TEST_CASE("uint128", "[math][uint128]")
{
	using uint128 = mh::uint128;

	constexpr uint128 val1(48927);
	REQUIRE(val1.u32[0] == 48927);
	REQUIRE(val1.u32[1] == 0);
	REQUIRE(val1.u32[2] == 0);
	REQUIRE(val1.u32[3] == 0);
	STATIC_REQUIRE(val1.get_u64<0>() == 48927);
	STATIC_REQUIRE(val1.get_u64<1>() == 0);

	//constexpr uint128 val2(8975);
	constexpr uint128 add_result = val1 + 8975;
	REQUIRE(add_result.u32[3] == 0);
	REQUIRE(add_result.u32[2] == 0);
	REQUIRE(add_result.u32[1] == 0);
	REQUIRE(add_result.u32[0] == 57902);
	STATIC_REQUIRE(add_result.get_u64<0>() == 57902);
	STATIC_REQUIRE(add_result.get_u64<1>() == 0);
	STATIC_REQUIRE(add_result == 57902U);

	{
		uint128 increment(0);
		REQUIRE(increment++ == 0U);
		REQUIRE(increment++ == 1U);
		REQUIRE(increment++ == 2U);
		REQUIRE(increment == 3U);
		REQUIRE(increment != 5U);
	}

	// Shift left
	{
		uint128 value(0xFFFFFFFF);
		value <<= 3;
		REQUIRE(value.get_u64<0>() == 0x7FFFFFFF8ULL);
		REQUIRE(value.get_u64<1>() == 0);
		value <<= 32;
		REQUIRE(value.get_u64<0>() == 0xFFFFFFF800000000ULL);
		REQUIRE(value.get_u64<1>() == 0x7);

		value = uint128(1U << 31);
		REQUIRE(value.get_u64<0>() == (1U << 31));
		REQUIRE(value.get_u64<1>() == 0);
		value <<= 1;
		REQUIRE(value.get_u64<0>() == (1ULL << 32));
		REQUIRE(value.get_u64<1>() == 0);

		constexpr uint128 value0 = uint128(1) << 0;
		STATIC_REQUIRE(value0.get_u64<0>() == 1);
		STATIC_REQUIRE(value0.get_u64<1>() == 0);
	}

	// Shift right
	{
		constexpr uint64_t MAX = 0xFFFFFFFFFFFFFFFF;
		constexpr uint128 value(MAX, MAX);
		STATIC_REQUIRE(value.get_u64<0>() == MAX);
		STATIC_REQUIRE(value.get_u64<1>() == MAX);

		constexpr uint128 shifted = value >> 127;
		STATIC_REQUIRE(shifted.get_u64<0>() == 1);
		STATIC_REQUIRE(shifted.get_u64<1>() == 0);

		constexpr uint128 shifted0 = uint128(MAX) >> 0;
		STATIC_REQUIRE(shifted0.get_u64<0>() == MAX);
		STATIC_REQUIRE(shifted0.get_u64<1>() == 0);
	}

	// Subtraction
	{
		uint128 value(0xFFFE);
		value -= 0x1111;
		REQUIRE(value.get_u64<0>() == 0xEEED);
		REQUIRE(value.get_u64<1>() == 0);
	}

	// Simple division
	{
		constexpr uint128 val3(0b100, 0b100);
		const auto div = val3 / 2;
		REQUIRE(div.u64[1] == (val3.u64[1] >> 1));
		REQUIRE(div.u64[0] == (val3.u64[0] >> 1));
	}

	constexpr uint64_t val2_constants[] =
	{
		17338555570256193913ULL,
		4867984972306000742ULL,
	};
	constexpr uint128 val2(val2_constants[0], val2_constants[1]);
	STATIC_REQUIRE(val2.get_u64<0>() == val2_constants[0]);
	STATIC_REQUIRE(val2.get_u64<1>() == val2_constants[1]);
	REQUIRE(val2.u32[0] == 1937615225);
	REQUIRE(val2.u32[1] == 4036947053);
	REQUIRE(val2.u32[2] == 1715284838);
	REQUIRE(val2.u32[3] == 1133416074);

	constexpr uint64_t val_mul_c0 = 0xFEAFFEAFFEAFFEAFULL;
	constexpr uint64_t val_mul_c1 = 0xADADADADADADADFFULL;//0xADDADAADADDADAADULL;
	constexpr auto val_mul0 = uint128::from_mul(val_mul_c0, val_mul_c1);
	STATIC_REQUIRE(val_mul0.get_u64<0>() == 0x17C60BB9FFADF351);
	STATIC_REQUIRE(val_mul0.get_u64<1>() == 0xACC9B8D5C4E1D13E);

	const auto val_div0 = val_mul0 / val_mul_c1;
	REQUIRE(val_div0.get_u64<0>() == val_mul_c0);
	REQUIRE(val_div0.get_u64<1>() == 0);
	constexpr auto val_div1 = val_mul0 / val_mul_c0;
	REQUIRE(val_div1.get_u64<0>() == val_mul_c1);
	REQUIRE(val_div1.get_u64<1>() == 0);
}

TEST_CASE("uint128 - comparisons with signed integers", "[math][uint128]")
{
	using uint128 = mh::uint128;

	// Plain int literals are signed; all comparison spellings must compile
	// (no narrowing errors in <=>) and order correctly.
	const uint128 v(5);
	REQUIRE(v < 6);
	REQUIRE(v <= 5);
	REQUIRE(v > 4);
	REQUIRE(v >= 0);
	REQUIRE(v == 5);
	REQUIRE(v != 6);
	REQUIRE(4 < v);
	REQUIRE(6 > v);
	REQUIRE(5 == v);

	// Negative values: any uint128 is greater
	REQUIRE(v > -1);
	REQUIRE(v >= -1);
	REQUIRE_FALSE(v < -1);
	REQUIRE_FALSE(v == -1);
	REQUIRE(v != -1);
	REQUIRE(-1 < v);
	REQUIRE_FALSE(-1 > v);
	REQUIRE(v > int8_t(-1));
	REQUIRE(v > int16_t(-1));
	REQUIRE(v > int32_t(-1));
	REQUIRE(v > int64_t(-1));
	REQUIRE(v > std::numeric_limits<int64_t>::min());
	REQUIRE(uint128(0) > -1); // even zero

	// Values needing the high 64 bits compare greater than any built-in signed value
	const uint128 big(0, 1); // 2^64
	REQUIRE(big > 5);
	REQUIRE(big > -5);
	REQUIRE(big > std::numeric_limits<int64_t>::max());
	REQUIRE_FALSE(big == 5);
	REQUIRE(big != 5);
	REQUIRE(5 < big);
	REQUIRE(-5 < big);

	// Everything above must also work in constant expressions
	constexpr uint128 c(7);
	STATIC_REQUIRE(c < 8);
	STATIC_REQUIRE(c > -3);
	STATIC_REQUIRE(-3 < c);
	STATIC_REQUIRE(c == 7);
	STATIC_REQUIRE(!(c == -7));
	STATIC_REQUIRE(c != -7);
	constexpr uint128 cbig(0, 2);
	STATIC_REQUIRE(cbig > -1);
	STATIC_REQUIRE(cbig > 1000000);

	// Unsigned comparisons keep working
	REQUIRE(v < 6u);
	REQUIRE(v == 5u);
	REQUIRE(v > 4ull);
	REQUIRE(big > 5u);
}

TEST_CASE("uint128 - division edge cases", "[math][uint128]")
{
	using uint128 = mh::uint128;

	// Dividend with a zero low half and the high half exactly divisible:
	// 2^65 / 2 == 2^64 (this shape used to shift a uint64 by its full width
	// in the portable long-division implementation).
	{
		constexpr uint128 quotient = uint128(0, 2) / 2;
		STATIC_REQUIRE(quotient.get_u64<1>() == 1);
		STATIC_REQUIRE(quotient.get_u64<0>() == 0);

		const uint128 runtime_quotient = uint128(0, 2) / 2;
		REQUIRE(runtime_quotient.get_u64<1>() == 1);
		REQUIRE(runtime_quotient.get_u64<0>() == 0);
	}

	// low == 0, high divisible by a larger divisor
	{
		constexpr uint128 quotient = uint128(0, 0xF0) / 16;
		STATIC_REQUIRE(quotient.get_u64<1>() == 0xF);
		STATIC_REQUIRE(quotient.get_u64<0>() == 0);
	}

	// low == 0, high NOT divisible: (3 * 2^64) / 2 == 2^64 + 2^63
	{
		constexpr uint128 quotient = uint128(0, 3) / 2;
		STATIC_REQUIRE(quotient.get_u64<1>() == 1);
		STATIC_REQUIRE(quotient.get_u64<0>() == uint64_t(1) << 63);
	}

	// low == 0, divisor with the top bit set: (2^63 * 2^64) / 2^63 == 2^64
	{
		constexpr uint128 quotient = uint128(0, uint64_t(1) << 63) / (uint64_t(1) << 63);
		STATIC_REQUIRE(quotient.get_u64<1>() == 1);
		STATIC_REQUIRE(quotient.get_u64<0>() == 0);
	}

	// Zero dividend
	{
		constexpr uint128 quotient = uint128(0, 0) / 12345;
		STATIC_REQUIRE(quotient == 0);
	}

	// Divisor larger than the dividend
	{
		constexpr uint128 quotient = uint128(100) / 12345;
		STATIC_REQUIRE(quotient == 0);
	}
}

TEST_CASE("uint128 - shifts by 128 or more yield zero", "[math][uint128]")
{
	using uint128 = mh::uint128;
	constexpr uint64_t MAX = 0xFFFFFFFFFFFFFFFFull;
	const uint128 ones(MAX, MAX);

	// The runtime (platform-intrinsic) path must agree with the documented
	// constexpr semantics: shifting everything out gives zero, never the
	// unchanged value (a full-width shift on a native 128-bit type is UB).
	REQUIRE((ones << 128) == uint128(0));
	REQUIRE((ones >> 128) == uint128(0));
	REQUIRE((ones << 129) == uint128(0));
	REQUIRE((ones >> 129) == uint128(0));
	REQUIRE((ones << 200) == uint128(0));
	REQUIRE((ones >> 200) == uint128(0));

	// Same in constant expressions
	{
		constexpr uint128 shifted_left = uint128(MAX, MAX) << 128;
		STATIC_REQUIRE(shifted_left == 0u);
		constexpr uint128 shifted_right = uint128(MAX, MAX) >> 128;
		STATIC_REQUIRE(shifted_right == 0u);
	}

	// Count types other than int
	REQUIRE((ones << uint8_t(128)) == uint128(0));
	REQUIRE((ones >> uint8_t(255)) == uint128(0));
	REQUIRE((ones << static_cast<unsigned short>(130)) == uint128(0));
	REQUIRE((ones << 128ll) == uint128(0));
	REQUIRE((ones >> size_t(128)) == uint128(0));

	// In-range shifts with narrow/wide count types still shift normally
	REQUIRE((ones << int8_t(64)) == uint128(0, MAX));
	REQUIRE((ones >> uint8_t(64)) == uint128(MAX, 0));
	REQUIRE((uint128(1) << size_t(127)) == uint128(0, uint64_t(1) << 63));
	REQUIRE((ones << 100ll) == (ones << 100));

	// Negative shift counts throw (matching the longstanding constexpr semantics)
	REQUIRE_THROWS(ones << -1);
	REQUIRE_THROWS(ones >> -1);
	REQUIRE_THROWS(ones << int8_t(-5));
	REQUIRE_THROWS(ones >> int64_t(-100));
}

TEST_CASE("uint128 - stream insertion restores formatting flags", "[math][uint128]")
{
	// The inserter prints in hex internally; it must not leave the stream's
	// basefield (or any other flags) modified afterwards.
	{
		std::ostringstream ss;
		ss << mh::uint128(255) << ' ' << 255;
		REQUIRE(ss.str() == "[0|ff] 255");
	}

	{
		// Caller-selected flags are preserved across the insertion
		std::ostringstream ss;
		ss << std::oct << 8 << ' ' << mh::uint128(255) << ' ' << 8;
		REQUIRE(ss.str() == "10 [0|ff] 10");
	}
}
