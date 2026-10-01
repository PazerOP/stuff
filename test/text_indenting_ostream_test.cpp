#include "mh/text/indenting_ostream.hpp"
#include <catch2/catch_all.hpp>

#include <sstream>
#include <string>

TEST_CASE("indented - indentation is added after newlines", "[text][indenting_ostream]")
{
	std::ostringstream os;
	os << mh::indented(std::string("line1\nline2"));
	REQUIRE(os.str() == "line1\n\tline2");
}

TEST_CASE("indented - the underlying stream remains usable for chaining", "[text][indenting_ostream]")
{
	// `os << indented(x) << "TAIL"` must evaluate to the original stream, not
	// to a temporary wrapper that is destroyed at the end of the statement
	std::ostringstream os;
	os << mh::indented(std::string("a\nb")) << "TAIL";
	REQUIRE(os.str() == "a\n\tbTAIL");

	std::ostream& returned = (os << mh::indented(std::string("!")));
	CHECK(&returned == &os);
}

TEST_CASE("indented - custom indent char and count", "[text][indenting_ostream]")
{
	std::ostringstream os;
	os << mh::indented(std::string("x\ny\nz"), '>', 2);
	REQUIRE(os.str() == "x\n>>y\n>>z");
}

TEST_CASE("indenting_ostream - direct use", "[text][indenting_ostream]")
{
	std::ostringstream os;

	{
		mh::indenting_ostream<char, std::char_traits<char>> indentingStream(os);
		indentingStream << "a\nb"; // block write (xsputn)
		indentingStream.put('\n'); // single-character write (overflow)
		indentingStream.put('c');
	}

	REQUIRE(os.str() == "a\n\tb\n\tc");
}
