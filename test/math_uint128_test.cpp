#include "mh/math/uint128.hpp"
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

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
