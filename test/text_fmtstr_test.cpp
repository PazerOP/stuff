#include "mh/text/fmtstr.hpp"
#include <catch2/catch_all.hpp>

#include <string>
#include <string_view>

TEST_CASE("printf_string - basic formatting and truncation", "[text][fmtstr]")
{
	const mh::pfstr<32> s("%s-%d", "abc", 42);
	REQUIRE(s.str() == "abc-42");
	CHECK(s.size() == 6);
	CHECK_FALSE(s.empty());

	// output longer than the buffer is truncated, never overflowed
	const mh::pfstr<4> t("%s", "abcdef");
	REQUIRE(t.str() == "abc");
	CHECK(t.size() == 3);
	CHECK(t.c_str()[3] == '\0');
}

TEST_CASE("format string buffers - assignment replaces the content", "[text][fmtstr]")
{
	mh::pfstr<32> s;
	s = "foo";
	REQUIRE(s.str() == "foo");

	s = "bar"; // must replace, not append
	REQUIRE(s.str() == "bar");
	CHECK(s.size() == 3);

	// same through the base class interface
	mh::base_format_string<32>& base = s;
	base = "baz";
	REQUIRE(s.str() == "baz");
	base = "qux";
	REQUIRE(s.str() == "qux");
	CHECK(s.size() == 3);
}

TEST_CASE("printf_string - conversion errors leave the buffer empty and reusable", "[text][fmtstr]")
{
	// %ls with a lone-surrogate wchar_t cannot be converted in the default "C"
	// locale; vsnprintf reports an error (negative return), which must not be
	// misinterpreted as a huge successful write
	const wchar_t invalidWide[] = { wchar_t(0xD800), 0 };
	mh::pfstr<16> s("%ls", invalidWide);
	CHECK(s.size() == 0);
	CHECK(s.view().empty());
	CHECK(s.c_str()[0] == '\0');

	// the object must remain fully usable afterwards
	s = "ok";
	REQUIRE(s.str() == "ok");
	s.sprintf("-%d", 7);
	REQUIRE(s.str() == "ok-7");
}

#if MH_FORMATTER != MH_FORMATTER_NONE
TEST_CASE("format_string - construct, assign, append", "[text][fmtstr]")
{
	const mh::fmtstr<32> s("{}-{}", "abc", 42);
	REQUIRE(s.str() == "abc-42");

	mh::fmtstr<32> t;
	t = "foo";
	t = "bar"; // must replace, not append
	REQUIRE(t.str() == "bar");

	t.fmt(" {}", 1); // fmt() explicitly appends
	REQUIRE(t.str() == "bar 1");
}
#endif // MH_FORMATTER != MH_FORMATTER_NONE
