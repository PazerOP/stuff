#include "mh/text/multi_char.hpp"
#include <catch2/catch_all.hpp>

TEST_CASE("multi_char stores all representations", "[text][multi_char]")
{
	// runtime (not constexpr) object so the accessors actually execute
	const mh::multi_char x = mh_make_multi_char(x);

	CHECK(x.get<char>() == 'x');
	CHECK(x.get<wchar_t>() == L'x');
#if __cpp_char8_t >= 201811
	CHECK(x.get<char8_t>() == u8'x');
#endif
	CHECK(x.get<char16_t>() == u'x');
	CHECK(x.get<char32_t>() == U'x');

	// pointer/array/reference types resolve to their character type
	CHECK(x.get<const wchar_t*>() == L'x');
	CHECK(x.get<const char*>() == 'x');
}

TEST_CASE("multi_char equality in both directions", "[text][multi_char]")
{
	const mh::multi_char x = mh_make_multi_char(x);

	CHECK(x == 'x');
	CHECK('x' == x);
	CHECK_FALSE(x == 'y');
	CHECK_FALSE('y' == x);

	CHECK(x == L'x');
	CHECK(L'x' == x);
	CHECK_FALSE(x == L'y');
	CHECK_FALSE(L'y' == x);

#if __cpp_char8_t >= 201811
	CHECK(x == u8'x');
	CHECK(u8'x' == x);
#endif

	CHECK(x == u'x');
	CHECK(u'x' == x);
	CHECK(x == U'x');
	CHECK(U'x' == x);
}

#if __has_include(<compare>)
TEST_CASE("multi_char ordering in both directions", "[text][multi_char]")
{
	const mh::multi_char m = mh_make_multi_char(m);

	CHECK(m < 'n');
	CHECK('l' < m);
	CHECK(m < L'n');
	CHECK(L'l' < m);

#if __cpp_char8_t >= 201811
	CHECK(m < u8'n');
	CHECK(u8'l' < m);
#endif

	CHECK(m < u'n');
	CHECK(u'l' < m);
	CHECK(m < U'n');
	CHECK(U'l' < m);

	CHECK(m <= 'm');
	CHECK(m >= 'm');
	CHECK(m > 'a');
}
#endif
