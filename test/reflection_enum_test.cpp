#include "mh/reflection/enum.hpp"
#include <catch2/catch_all.hpp>

#include <cstdint>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mh_enum_test
{
	enum class sound : int
	{
		beep = 0,
		boop = 1,
		honk = 50,
		silence = -1,           // negative values must reflect too
		loudest = 2147483647,   // edge of the underlying type
	};
}

MH_ENUM_REFLECT_BEGIN(mh_enum_test::sound)
	MH_ENUM_REFLECT_VALUE(beep)
	MH_ENUM_REFLECT_VALUE(boop)
	MH_ENUM_REFLECT_VALUE(honk)
	MH_ENUM_REFLECT_VALUE(silence)
	MH_ENUM_REFLECT_VALUE(loudest)
MH_ENUM_REFLECT_END()

namespace
{
	// unscoped, not in a namespace as far as the reflection name is concerned,
	// with a small unsigned underlying type
	enum global_flag : std::uint8_t
	{
		flag_off,
		flag_on,
	};
}

MH_ENUM_REFLECT_BEGIN(global_flag)
	MH_ENUM_REFLECT_VALUE(flag_off)
	MH_ENUM_REFLECT_VALUE(flag_on)
MH_ENUM_REFLECT_END()

using mh_enum_test::sound;
using sound_type = mh::enum_type<sound>;
using flag_type = mh::enum_type<global_flag>;

TEST_CASE("enum reflection - type names", "[reflection][enum]")
{
	CHECK(std::string(sound_type::type_name_full()) == "mh_enum_test::sound");
	CHECK(std::string(sound_type::type_name()) == "sound"); // namespace stripped

	// no namespace: full and short names are the same
	CHECK(std::string(flag_type::type_name_full()) == "global_flag");
	CHECK(std::string(flag_type::type_name()) == "global_flag");
}

TEST_CASE("enum reflection - value -> name", "[reflection][enum]")
{
	CHECK(sound_type::try_find_name(sound::beep) == "beep");
	CHECK(sound_type::try_find_name(sound::silence) == "silence");
	CHECK(sound_type::try_find_name(sound::loudest) == "loudest");
	CHECK(sound_type::try_find_name(sound(1234)).empty()); // miss: empty view

	CHECK(sound_type::find_name(sound::honk) == "honk");
	CHECK_THROWS_AS(sound_type::find_name(sound(1234)), std::invalid_argument);

	// the reflection is also usable at compile time
	STATIC_REQUIRE(sound_type::try_find_name(sound::beep) == "beep");
}

TEST_CASE("enum reflection - name -> value", "[reflection][enum]")
{
	const auto boop = sound_type::try_find_value("boop");
	REQUIRE(boop.has_value());
	CHECK(*boop == sound::boop);
	CHECK_FALSE(sound_type::try_find_value("nope").has_value());

	CHECK(sound_type::find_value("silence") == sound::silence);
	CHECK_THROWS_AS(sound_type::find_value("nope"), std::invalid_argument);
}

TEST_CASE("enum reflection - free helper functions", "[reflection][enum]")
{
	CHECK(mh::find_enum_value_name(sound::beep) == "beep");
	CHECK(mh::try_find_enum_value_name(sound::honk) == "honk");
	CHECK(mh::try_find_enum_value_name(sound(99)).empty());

	CHECK(mh::find_enum_value<sound>("honk") == sound::honk);
	CHECK_THROWS_AS(mh::find_enum_value<sound>("nope"), std::invalid_argument);

	sound byRef{};
	mh::find_enum_value("boop", byRef);
	CHECK(byRef == sound::boop);

	CHECK(mh::try_find_enum_value<sound>("silence") == sound::silence);
	CHECK_FALSE(mh::try_find_enum_value<sound>("nope").has_value());

	sound found{};
	CHECK(mh::try_find_enum_value("honk", found));
	CHECK(found == sound::honk);

	sound untouched = sound::beep;
	CHECK_FALSE(mh::try_find_enum_value("nope", untouched));
	CHECK(untouched == sound::beep); // a miss must not modify the output
}

TEST_CASE("enum reflection - enum_value accessors", "[reflection][enum]")
{
	REQUIRE(std::size(sound_type::VALUES) == 5);

	const mh::enum_value<sound>& first = sound_type::VALUES[0];
	CHECK(first.value() == sound::beep);
	CHECK(first.value_name() == "beep");
	CHECK(first.underlying_value() == 0);
	CHECK(first.type_name() == "sound");

	const mh::enum_value<sound>& negative = sound_type::VALUES[3];
	CHECK(negative.value() == sound::silence);
	CHECK(negative.underlying_value() == -1);
}

TEST_CASE("enum reflection - enum_fmt ostream insertion", "[reflection][enum]")
{
	CHECK(mh::enum_fmt(sound::beep).m_Value == sound::beep);

	// named value: type name, "::", value name (same shape as the formatter)
	{
		std::ostringstream os;
		os << mh::enum_fmt(sound::boop);
		CHECK(os.str() == "sound::boop");
	}

	// unnamed value: type name and the numeric value in parentheses
	{
		std::ostringstream os;
		os << mh::enum_fmt(sound(1234));
		CHECK(os.str() == "sound(1234)");
	}

	// negative unnamed value
	{
		std::ostringstream os;
		os << mh::enum_fmt(sound(-42));
		CHECK(os.str() == "sound(-42)");
	}

	// small unsigned underlying types must print numerically, not as characters
	{
		std::ostringstream os;
		os << mh::enum_fmt(global_flag(200));
		CHECK(os.str() == "global_flag(200)");
	}

	{
		std::ostringstream os;
		os << mh::enum_fmt(flag_on);
		CHECK(os.str() == "global_flag::flag_on");
	}
}

#if MH_FORMATTER != MH_FORMATTER_NONE

TEST_CASE("enum reflection - enum_fmt formatter", "[reflection][enum][format]")
{
	// default: short type name + value
	CHECK(mh::format("{}", mh::enum_fmt(sound::beep)) == "sound::beep");
	CHECK(mh::format("{:tv}", mh::enum_fmt(sound::beep)) == "sound::beep");

	// unnamed values fall back to the numeric form
	CHECK(mh::format("{}", mh::enum_fmt(sound(1234))) == "sound(1234)");

	// T: fully qualified type name
	CHECK(mh::format("{:Tv}", mh::enum_fmt(sound::honk)) == "mh_enum_test::sound::honk");

	// v alone: just the value
	CHECK(mh::format("{:v}", mh::enum_fmt(sound::beep)) == "beep");
	CHECK(mh::format("{:v}", mh::enum_fmt(sound(1234))) == "1234");

	// t/T without v still shows the numeric value in parentheses
	CHECK(mh::format("{:t}", mh::enum_fmt(sound::beep)) == "sound(0)");
}

TEST_CASE("enum reflection - enum_fmt formatter rejects invalid specs", "[reflection][enum][format]")
{
	// 't' and 'T' are mutually exclusive
	CHECK_THROWS_AS(mh::format("{:tT}", mh::enum_fmt(sound::beep)), mh::format_error);
	CHECK_THROWS_AS(mh::format("{:Tt}", mh::enum_fmt(sound::beep)), mh::format_error);

	// unknown presentation character
	CHECK_THROWS_AS(mh::format("{:q}", mh::enum_fmt(sound::beep)), mh::format_error);

	// unterminated format spec
	CHECK_THROWS_AS(mh::format("{:t", mh::enum_fmt(sound::beep)), mh::format_error);
}

// NOTE: the wide (CharT != char) branch of the enum_fmt formatter cannot be
// tested: instantiating it does not compile against fmt 9 (mh::format_to wraps
// the format string in fmt::runtime, which fmt 9's wide format_to overloads do
// not accept). Recorded as an open question for the maintainer.

#endif // MH_FORMATTER != MH_FORMATTER_NONE
