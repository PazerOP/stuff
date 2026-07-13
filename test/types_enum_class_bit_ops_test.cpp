#include "mh/types/enum_class_bit_ops.hpp"

#include <catch2/catch_all.hpp>

#include <cstdint>

namespace
{
	enum class flags : uint8_t
	{
		none = 0,
		a = 1 << 0,
		b = 1 << 1,
		c = 1 << 2,
	};
	MH_ENABLE_ENUM_CLASS_BIT_OPS(flags);
}

TEST_CASE("enum class bit ops", "[types][enum_class_bit_ops]")
{
	// operator| combines, operator& masks; both convert back to the enum
	constexpr flags ab = flags::a | flags::b;
	STATIC_CHECK(uint8_t(flags(ab)) == 0b011);
	STATIC_CHECK(uint8_t(flags(ab & flags::a)) == 0b001);
	STATIC_CHECK(uint8_t(flags(ab & flags::c)) == 0);

	// the result wrapper is truthy exactly when any bit survived,
	// enabling `if (x & flag)` tests
	STATIC_CHECK(static_cast<bool>(ab & flags::a));
	STATIC_CHECK_FALSE(static_cast<bool>(ab & flags::c));
	CHECK(static_cast<bool>(flags::a | flags::none));
	CHECK_FALSE(static_cast<bool>(flags::none & flags::none));
	if (ab & flags::b)
		SUCCEED("bit test reads naturally");
	else
		FAIL("expected flags::b to be set");

	// compound assignment mutates the lhs and returns the new value
	flags value = flags::a;
	value |= flags::c;
	CHECK(uint8_t(value) == 0b101);
	CHECK(static_cast<bool>(value &= flags::c));
	CHECK(uint8_t(value) == 0b100);
	CHECK_FALSE(static_cast<bool>(value &= flags::b));
	CHECK(uint8_t(value) == 0);
}
