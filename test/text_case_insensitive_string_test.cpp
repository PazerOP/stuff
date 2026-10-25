#include "mh/text/case_insensitive_string.hpp"
#include <catch2/catch_all.hpp>

template<typename CharT = char, typename Traits = std::char_traits<CharT>>
	inline auto test_view(const std::basic_string_view<CharT, Traits>& sv)
	{
		return std::basic_string_view<CharT, mh::case_insensitive_char_traits<Traits>>(sv.data(), sv.size());
	}

TEST_CASE("case insensitive string", "[text][case_insensitive_string]")
{
	using namespace std::string_literals;
	using namespace std::string_view_literals;

	//std::basic_string_view<char, std::char_traits<char>> testBase = "hello string view";
	//auto test = mh::case_insensitive_view("hello string view");
	//auto test2 = mh::case_insensitive_string("hello string");
	//auto test3 = test_view<char, std::char_traits<char>>("hello string");
	REQUIRE(mh::case_insensitive_view("hello world") == mh::case_insensitive_view("HELLO world"));
	REQUIRE(mh::case_insensitive_view("hello world"s) == mh::case_insensitive_view("HELLO world"s));
	REQUIRE(mh::case_insensitive_view("hello world"sv) == mh::case_insensitive_view("HELLO world"sv));
	REQUIRE(mh::case_insensitive_string("hello world") == mh::case_insensitive_string("HELLO world"));
	REQUIRE(mh::case_insensitive_string("hello world"s) == mh::case_insensitive_string("HELLO world"s));
	REQUIRE(mh::case_insensitive_string("hello world"sv) == mh::case_insensitive_string("HELLO world"sv));
}

TEST_CASE("case insensitive find", "[text][case_insensitive_string]")
{
	// basic_string_view::find(CharT), find_first_of, etc. all route through
	// traits::find, which must match case-insensitively
	const auto haystack = mh::case_insensitive_view("HELLO");
	CHECK(haystack.find('h') == 0);
	CHECK(haystack.find('L') == 2);
	CHECK(haystack.find('o') == 4);
	CHECK(haystack.find('z') == haystack.npos);
	CHECK(haystack.find_first_of("le") == 1);
}

TEST_CASE("case insensitive find - embedded null, exact count", "[text][case_insensitive_string]")
{
	// traits::find must examine exactly `count` characters: it may neither stop
	// early at an embedded '\0' nor be unable to find '\0' itself
	constexpr char raw[] = { 'A', '\0', 'b' };
	const auto v = mh::case_insensitive_view(raw, 3);
	CHECK(v.find('a') == 0);
	CHECK(v.find('\0') == 1);
	CHECK(v.find('B') == 2);
}

TEST_CASE("case insensitive traits - bytes above 0x7F", "[text][case_insensitive_string]")
{
	// Bytes >= 0x80 are negative on signed-char platforms; the traits must
	// treat them as unsigned char (both for correct ordering and because
	// passing negative values to std::toupper is undefined behavior)
	using traits = mh::case_insensitive_char_traits<std::char_traits<char>>;

	constexpr char highByte = '\xE9';
	CHECK(traits::eq(highByte, highByte));
	CHECK_FALSE(traits::lt(highByte, 'A')); // 0xE9 (233) orders after 'A' (65)
	CHECK(traits::lt('A', highByte));
	CHECK(traits::compare(&highByte, "A", 1) > 0);
	CHECK(traits::compare("A", &highByte, 1) < 0);

	constexpr char rawWithHighByte[] = { '\xE9', 'x' };
	const auto v = mh::case_insensitive_view(rawWithHighByte, 2);
	CHECK(v.find('\xE9') == 0);
	CHECK(v.find('X') == 1);
}

TEST_CASE("case insensitive traits - wchar_t", "[text][case_insensitive_string]")
{
	// The wide instantiation must use the wide character classification
	// functions; the narrow std::toupper only accepts unsigned char/EOF values
	using wtraits = mh::case_insensitive_char_traits<std::char_traits<wchar_t>>;

	CHECK(wtraits::eq(L'h', L'H'));
	CHECK_FALSE(wtraits::eq(L'h', L'i'));
	CHECK(wtraits::compare(L"HeLLo", L"hEllO", 5) == 0);
	CHECK(wtraits::compare(L"abc", L"ABD", 3) < 0);

	// values far outside the narrow toupper's [0, 255]+EOF domain must be safe
	CHECK(wtraits::eq(L'\x3B1', L'\x3B1'));
	CHECK_FALSE(wtraits::eq(L'\x3B1', L'A'));

	// lt compares the uppercased characters
	CHECK(wtraits::lt(L'a', L'B'));
	CHECK_FALSE(wtraits::lt(L'b', L'A'));
	CHECK_FALSE(wtraits::lt(L'a', L'A')); // equal after uppercasing

	const auto wv = mh::case_insensitive_view(L"WORLD");
	CHECK(wv.find(L'w') == 0);
	CHECK(wv.find(L'D') == 4);
	CHECK(wv.find(L'q') == wv.npos);

	REQUIRE(mh::case_insensitive_view(L"hello world") == mh::case_insensitive_view(L"HELLO WORLD"));
}

TEST_CASE("case insensitive traits - construction from the base traits", "[text][case_insensitive_string]")
{
	using traits = mh::case_insensitive_char_traits<std::char_traits<char>>;
	using wtraits = mh::case_insensitive_char_traits<std::char_traits<wchar_t>>;

	const std::char_traits<char> narrowBase{};
	const traits fromCopy(narrowBase);
	const traits fromMove(std::char_traits<char>{});
	CHECK(fromCopy.eq('x', 'X'));
	CHECK(fromMove.eq('y', 'Y'));

	const std::char_traits<wchar_t> wideBase{};
	const wtraits wideFromCopy(wideBase);
	const wtraits wideFromMove(std::char_traits<wchar_t>{});
	CHECK(wideFromCopy.eq(L'x', L'X'));
	CHECK(wideFromMove.eq(L'y', L'Y'));
}
