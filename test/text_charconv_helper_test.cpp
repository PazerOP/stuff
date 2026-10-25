#include "mh/text/charconv_helper.hpp"
#include <catch2/catch_all.hpp>

#if __cpp_lib_to_chars >= 201611

#include <optional>
#include <string>
#include <string_view>

TEST_CASE("from_chars - integers", "[text][charconv_helper]")
{
	int value = -1;
	REQUIRE(mh::from_chars(std::string_view("123"), value));
	REQUIRE(value == 123);

	REQUIRE(!mh::from_chars(std::string_view("abc"), value));

	size_t charsRead = 12345;
	std::optional<int> opt = mh::from_chars<int>(std::string_view("123"), &charsRead);
	REQUIRE(opt.has_value());
	REQUIRE(*opt == 123);
	REQUIRE(charsRead == 3);

	charsRead = 12345;
	opt = mh::from_chars<int>(std::string_view("42abc"), &charsRead);
	REQUIRE(opt.has_value());
	REQUIRE(*opt == 42);
	REQUIRE(charsRead == 2);

	REQUIRE(!mh::from_chars<int>(std::string_view("abc")).has_value());
}

TEST_CASE("from_chars - floats", "[text][charconv_helper]")
{
	float value = -1;
	REQUIRE(mh::from_chars(std::string_view("1.5"), value));
	REQUIRE(value == 1.5f);

	size_t charsRead = 12345;
	std::optional<float> opt = mh::from_chars<float>(std::string_view("1.5"), &charsRead);
	REQUIRE(opt.has_value());
	REQUIRE(*opt == 1.5f);
	REQUIRE(charsRead == 3);

	REQUIRE(!mh::from_chars<float>(std::string_view("xyz")).has_value());
}

TEST_CASE("from_chars - bool", "[text][charconv_helper]")
{
	bool value = false;
	REQUIRE(mh::from_chars(std::string_view("1"), value));
	REQUIRE(value == true);

	REQUIRE(mh::from_chars(std::string_view("0"), value));
	REQUIRE(value == false);

	REQUIRE(!mh::from_chars(std::string_view("2"), value));

	// a bool parse whose underlying integer parse fails propagates that
	// failure (not just the out-of-range rejection above)
	REQUIRE(!mh::from_chars(std::string_view("x"), value));
	REQUIRE(!mh::from_chars(std::string_view(""), value));
}

TEST_CASE("to_chars", "[text][charconv_helper]")
{
	char buf[32];
	const auto intResult = mh::to_chars(buf, 1234);
	REQUIRE(intResult);
	REQUIRE(std::string(buf, intResult.ptr) == "1234");

	const auto floatResult = mh::to_chars(buf, 1.5f, std::chars_format::general);
	REQUIRE(floatResult);
	REQUIRE(std::string(buf, floatResult.ptr) == "1.5");

	const auto precisionResult = mh::to_chars(buf, 1.5f, std::chars_format::fixed, 3);
	REQUIRE(precisionResult);
	REQUIRE(std::string(buf, precisionResult.ptr) == "1.500");

	char tiny[2];
	REQUIRE(!mh::to_chars(tiny, 123456));
}

#endif
