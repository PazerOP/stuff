#include "mh/data/bit_float.hpp"
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <sstream>

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

	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b0110100000000000)) == Catch::Approx(2048));
	REQUIRE(half_float::native_to_bits(2048) == half_float::bits_t(0b0110100000000000));
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b0110001110101100)) == Catch::Approx(982));
	REQUIRE(half_float::native_to_bits(982) == half_float::bits_t(0b0110001110101100));
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b1101101000001011)) == Catch::Approx(-193.4).epsilon(0.05f));

	{
		constexpr auto bits = half_float::bits_t(0b1001010100000000);
		static_assert(half_float::bits_to_exponent(bits) == half_float::exponent_t(5));
		static_assert(half_float::bits_to_mantissa(bits) == half_float::mantissa_t(0b0100000000));
		REQUIRE(half_float::bits_to_native(bits) == Catch::Approx(-0.0012207031));
	}

	REQUIRE(half_float::bits_to_native(half_float::bits_t(0b0101110010011001)) == Catch::Approx(294.25));
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
		REQUIRE(rt_value == Catch::Approx(value).epsilon(epsilon));
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

	REQUIRE(double(half_float::bits_to_native(nlh::min())) == Catch::Approx(6e-5).epsilon(0.0175));
}

TEST_CASE("bit_float - roundtrip inf/nan")
{
	//constexpr float inf = std::numeric_limits<float>::infinity();
	//constexpr float nan = std::numeric_limits<float>::quiet_NaN();

	// TODO
	//const auto halfbits_inf = half_float::native_to_bits(inf);
	//const auto halfbits_nan = half_float::native_to_bits(nan);
}

TEST_CASE("bit_float - narrowing exponent underflow flushes to zero", "[bit_float]")
{
	// Values below half's normal/denormal range must encode as +-0. They must
	// NEVER wrap around to an in-range or all-ones (infinity/NaN) exponent.
	// 2e-5f in particular used to come back as +infinity.
	for (const float value : { 2e-5f, 1e-8f, 1e-10f, 1e-20f, 1e-30f,
		5.9604645e-8f /* == half's smallest denormal; flush-to-zero semantics */ })
	{
		CAPTURE(value);
		REQUIRE(uint16_t(half_float::native_to_bits(value)) == 0x0000);
		REQUIRE(uint16_t(half_float::native_to_bits(-value)) == 0x8000); // sign preserved
	}

	// float denormals are far below half's entire range: also +-0
	REQUIRE(uint16_t(half_float::native_to_bits(1e-40f)) == 0x0000);
	REQUIRE(uint16_t(half_float::native_to_bits(-1e-40f)) == 0x8000);
}

TEST_CASE("bit_float - half denormals decode to their actual values", "[bit_float]")
{
	// A half denormal encodes mantissa * 2^-24; the float result is a normal
	// float and must be produced exactly (not a raw-shifted float denormal).
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0x0001)) == std::ldexp(1.0f, -24)); // 5.9604645e-8
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0x03FF)) == std::ldexp(1023.0f, -24)); // largest denormal
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0x0155)) == std::ldexp(float(0x155), -24));
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0x8001)) == -std::ldexp(1.0f, -24)); // negative denormal
	REQUIRE(half_float::bits_to_native(half_float::bits_t(0x83FF)) == -std::ldexp(1023.0f, -24));
}

namespace
{
	// Reference IEEE 754 binary16 -> float decode
	float reference_half_decode(uint16_t h)
	{
		const uint32_t sign = (h >> 15) & 1;
		const uint32_t exponent = (h >> 10) & 0x1F;
		const uint32_t mantissa = h & 0x3FF;

		float value;
		if (exponent == 0)
			value = std::ldexp(float(mantissa), -24); // zero or denormal
		else if (exponent == 31)
			value = mantissa ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
		else
			value = std::ldexp(1.0f + mantissa / 1024.0f, int(exponent) - 15);

		return sign ? -value : value;
	}
}

TEST_CASE("bit_float - exhaustive half->float->half sweep", "[bit_float]")
{
	// All 65536 half patterns: bits_to_native must match the reference decode
	// bit-for-bit (NaN compares by NaN-ness), and every value that is exactly
	// representable in half (normals, zeros, infinities) must round-trip back
	// to the identical bit pattern. Runs in milliseconds.
	int decode_mismatches = 0;
	int roundtrip_mismatches = 0;
	uint32_t first_decode_mismatch = ~0u;
	uint32_t first_roundtrip_mismatch = ~0u;

	for (uint32_t i = 0; i <= 0xFFFF; i++)
	{
		const uint16_t pattern = uint16_t(i);
		const float decoded = half_float::bits_to_native(half_float::bits_t(pattern));
		const float expected = reference_half_decode(pattern);

		const bool decode_ok = std::isnan(expected)
			? std::isnan(decoded)
			: std::bit_cast<uint32_t>(decoded) == std::bit_cast<uint32_t>(expected);
		if (!decode_ok)
		{
			decode_mismatches++;
			first_decode_mismatch = std::min(first_decode_mismatch, i);
		}

		const uint32_t exponent = (pattern >> 10) & 0x1F;
		if (exponent != 31 && (exponent != 0 || (pattern & 0x3FF) == 0)) // normals and +-0
		{
			const auto reencoded = uint16_t(half_float::native_to_bits(decoded));
			if (reencoded != pattern)
			{
				roundtrip_mismatches++;
				first_roundtrip_mismatch = std::min(first_roundtrip_mismatch, i);
			}
		}
	}

	CAPTURE(first_decode_mismatch, first_roundtrip_mismatch);
	REQUIRE(decode_mismatches == 0);
	REQUIRE(roundtrip_mismatches == 0);
}

TEST_CASE("bit_float - mixed float/double-sized mantissa and exponent widths", "[bit_float]")
{
	// Formats whose mantissa fits float's but whose exponent does not (and vice
	// versa) must go through a native-layout intermediate that matches an actual
	// native type; conversions through a 23+11-bit hybrid silently produce garbage.
	{
		using wide_exponent = mh::bit_float<10, 11, true>;
		static_assert(std::is_same_v<wide_exponent::native_t, double>);
		REQUIRE(wide_exponent::bits_to_native(wide_exponent::native_to_bits(1.0)) == 1.0);
		REQUIRE(wide_exponent::bits_to_native(wide_exponent::native_to_bits(2.5)) == 2.5);
		REQUIRE(wide_exponent::bits_to_native(wide_exponent::native_to_bits(-0.375)) == -0.375);
	}

	{
		using wide_mantissa = mh::bit_float<30, 8, true>;
		static_assert(std::is_same_v<wide_mantissa::native_t, double>);
		REQUIRE(wide_mantissa::bits_to_native(wide_mantissa::native_to_bits(1.0)) == 1.0);
		REQUIRE(wide_mantissa::bits_to_native(wide_mantissa::native_to_bits(2.5)) == 2.5);
	}
}

TEST_CASE("bit_float - mantissa_t supports every width up to 52", "[bit_float]")
{
	// Width 32 needs a mask computed without shifting a 32-bit type by 32.
	STATIC_REQUIRE(mh::mantissa_t<32>::MASK == 0xFFFFFFFFu);
	STATIC_REQUIRE(std::is_same_v<mh::mantissa_t<32>::value_t, uint32_t>);
	STATIC_REQUIRE(mh::mantissa_t<32>(mh::mantissa_t<32>::MASK).value == 0xFFFFFFFFu);

	// Neighboring widths keep their masks
	STATIC_REQUIRE(mh::mantissa_t<31>::MASK == 0x7FFFFFFFu);
	STATIC_REQUIRE(mh::mantissa_t<33>::MASK == 0x1FFFFFFFFull);
	STATIC_REQUIRE(mh::mantissa_t<10>::MASK == 0x3FFu);
	STATIC_REQUIRE(mh::mantissa_t<52>::MASK == 0xFFFFFFFFFFFFFull);
}

TEST_CASE("bit_float - bits_t stream insertion", "[bit_float]")
{
	// bits_t values must be printable (as their decoded native value); the
	// operator has to be discoverable via ADL since the enclosing bit_float's
	// template arguments are not deducible from the enum type alone.
	{
		std::ostringstream ss;
		ss << half_float::bits_t(0x3C00); // 1.0 in binary16
		REQUIRE(ss.str() == "1");
	}

	{
		// A second instantiation coexists in the same translation unit
		std::ostringstream ss;
		ss << native_float::bits_t(0x40200000); // 2.5f
		REQUIRE(ss.str() == "2.5");
	}
}
