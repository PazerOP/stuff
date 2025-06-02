#include "mh/text/case_insensitive_string.hpp"
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

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
