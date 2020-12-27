#include "mh/data/bit_float.hpp"
#include "catch2/repo/single_include/catch2/catch.hpp"

using half_float = mh::half_float;
using native_float = mh::native_float;
using gl_float14 = mh::bit_float<9, 5, false>;
using gl_float11 = mh::bit_float<6, 5, false>;
using gl_float10 = mh::bit_float<5, 5, false>;

TEST_CASE("bit_float - half")
{
	static_assert(half_float::mantissa_t(0) == mh::mantissa_t<22>(0));
	static_assert(half_float::exponent_t(0) == mh::exponent_t<8>(0));

	REQUIRE(half_float::bits_to_sign(half_float::native_to_bits(-306)) == true);
	REQUIRE(half_float::bits_to_sign(half_float::native_to_bits(306)) == false);

	{
		constexpr auto bits = half_float::bits_t(0);
		const float native = half_float::bits_to_native(bits);

		uint32_t native_bits = *reinterpret_cast<const uint32_t*>(&native);
		REQUIRE(native_bits == 0);

		static_assert(half_float::bits_to_exponent(bits).value == 0);
		static_assert(half_float::bits_to_mantissa(bits).value == 0);
		static_assert(half_float::bits_to_sign(bits) == false);
		REQUIRE(native == 0);
	}

	{
		const float value = 982.0f;
		const auto bits = native_float::native_to_bits(value);
		REQUIRE(bits == *reinterpret_cast<const native_float::bits_t*>(&value));
		static_assert(half_float::exponent_t(0b11000) == native_float::exponent_t(0b10001000));
		static_assert(native_float::exponent_t(0b10001000) == half_float::exponent_t(0b11000));
		static_assert(half_float::mantissa_t(0b1110101100) == native_float::mantissa_t(0b11101011000000000000000));
		static_assert(native_float::mantissa_t(0b11101011000000000000000) == half_float::mantissa_t(0b1110101100));

		REQUIRE(native_float::bits_to_exponent(bits) == native_float::exponent_t(0b10001000));
		REQUIRE(native_float::bits_to_mantissa(bits) == native_float::mantissa_t(0b11101011000000000000000));
		REQUIRE(native_float::bits_to_sign(bits) == false);
		REQUIRE(native_float::bits_to_native(bits) == value);
	}

	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b0110100000000000)) == Approx(2048));
	REQUIRE(half_float::native_to_bits(2048) == half_float::bits_t(0b0110100000000000));
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b0110001110101100)) == Approx(982));
	REQUIRE(half_float::native_to_bits(982) == half_float::bits_t(0b0110001110101100));
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b1101101000001011)) == Approx(-193.4).epsilon(0.05f));

	{
		constexpr auto bits = half_float::bits_t(0b1001010100000000);
		static_assert(half_float::bits_to_exponent(bits) == half_float::exponent_t(5));
		static_assert(half_float::bits_to_mantissa(bits) == half_float::mantissa_t(0b0100000000));
		REQUIRE(half_float::bits_to_native(bits) == Approx(-0.0012207031));
	}

	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b0101110010011001)) == Approx(294.25));
	REQUIRE(half_float::native_to_bits(294.25) == half_float::bits_t(0b0101110010011001));
}

template<typename bf>
static void BasicBFTest()
{
	CAPTURE(typeid(bf).name());
	using bits_t = typename bf::bits_t;

	REQUIRE(bf::bits_to_native(bits_t(0)) == 0);
	REQUIRE(bf::native_to_bits(0) == bits_t(0));
	REQUIRE(bf::native_to_bits(-1) == bits_t(0));

	const auto TestRoundTrip = [](float value, float epsilon = 0.001)
	{
		const auto bits = bf::native_to_bits(value);
		const auto rt_value = bf::bits_to_native(bits);
		REQUIRE(rt_value == Approx(value).epsilon(epsilon));
	};

	TestRoundTrip(0.1, 0.01);
	TestRoundTrip(0.4, 0.01);
	TestRoundTrip(0.5, 0.01);
	TestRoundTrip(1, 0);
}

TEST_CASE("bit_float - gl")
{
	BasicBFTest<gl_float11>();
	BasicBFTest<gl_float10>();
}

TEST_CASE("bit_float - overflow to infinity")
{
	static_assert(native_float::exponent_t((1 << 8) - 1) == mh::exponent_t<7>(((1 << 7) - 1)));
	static_assert(mh::exponent_t<7>((1 << 7) - 1) == mh::exponent_t<8>((1 << 8) - 1));

	constexpr auto value = std::numeric_limits<float>::max();
	const auto fullbits = native_float::native_to_bits(value);
	const auto fullbits_cast = *reinterpret_cast<const uint32_t*>(&value);
	REQUIRE(fullbits_cast == uint32_t(fullbits));
	REQUIRE(native_float::bits_to_native(fullbits) == value);

	static_assert(native_float::exponent_t::zero().value == 127);
	static_assert(half_float::exponent_t::max().actual_value() == 15);
	const auto native_exponent = native_float::bits_to_exponent(fullbits);
	REQUIRE(native_exponent.value == 0xFE);
	REQUIRE(((unsigned(fullbits) >> 23) & 0xFF) == 0xFE);

	static_assert(native_float::exponent_t::inf_or_nan().value == 255);
	static_assert(half_float::exponent_t::inf_or_nan().value == 31);
	REQUIRE(+native_float::exponent_t::inf_or_nan().actual_value() == 128);

	const auto halfbits = half_float::native_to_bits(std::numeric_limits<float>::max());
	CAPTURE(halfbits);
	REQUIRE(half_float::is_inf(halfbits));

	const auto native = half_float::bits_to_native(halfbits);
	CAPTURE(native);
	REQUIRE(std::isinf(native));
}

template<typename bit_float_t> static void bit_float_numeric_limits_helper()
{
	using b = std::numeric_limits<bit_float_t>;
	using n = std::numeric_limits<typename bit_float_t::native_t>;

	static_assert(b::is_specialized == n::is_specialized);

	static_assert(b::max_exponent == n::max_exponent);
	static_assert(b::min_exponent == n::min_exponent);
	static_assert(b::digits == n::digits);
	static_assert(b::digits10 == n::digits10);
	static_assert(b::max_digits10 == n::max_digits10);
	static_assert(b::radix == n::radix);

	REQUIRE(bit_float_t::bits_to_native(b::max()) == n::max());
	REQUIRE(bit_float_t::bits_to_native(b::min()) == n::min());
}

TEST_CASE("bit_float - numeric_limits", "[bit_float]")
{
	bit_float_numeric_limits_helper<mh::native_float>();
	bit_float_numeric_limits_helper<mh::native_double>();

	using nlh = std::numeric_limits<mh::half_float>;
	static_assert(nlh::is_specialized);
	static_assert(nlh::is_signed);
	static_assert(!nlh::is_integer);
	static_assert(!nlh::is_exact);
	static_assert(nlh::has_infinity);
	static_assert(nlh::has_quiet_NaN);
	static_assert(!nlh::has_signaling_NaN);
	static_assert(nlh::has_denorm == (std::numeric_limits<float>::has_denorm == std::float_denorm_style::denorm_present));
	static_assert(nlh::has_denorm_loss == std::numeric_limits<float>::has_denorm_loss);
	static_assert(nlh::round_style == std::float_round_style::round_toward_zero);

	static_assert(nlh::is_iec559);
	static_assert(!std::numeric_limits<gl_float10>::is_iec559);

	static_assert(nlh::is_bounded);
	static_assert(!nlh::is_modulo);

	static_assert(nlh::digits == 11);
	static_assert(nlh::digits10 == 3);

	static_assert(nlh::min_exponent == -13);
	static_assert(nlh::max_exponent == 16);

	static_assert(!nlh::traps);

	REQUIRE(double(half_float::bits_to_native(nlh::min())) == Approx(6e-5).epsilon(0.0175));
}

TEST_CASE("bit_float - roundtrip inf/nan")
{
	//constexpr float inf = std::numeric_limits<float>::infinity();
	//constexpr float nan = std::numeric_limits<float>::quiet_NaN();

	// TODO
	//const auto halfbits_inf = half_float::native_to_bits(inf);
	//const auto halfbits_nan = half_float::native_to_bits(nan);
}
