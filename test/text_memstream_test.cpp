#include "mh/text/memstream.hpp"
#include <catch2/catch_all.hpp>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include "last_include.hpp"

// Helper to convert string_view to string for Catch2 comparisons
// (Catch2 v3.4.0 declares but doesn't implement StringMaker<std::string_view>)
inline std::string to_str(std::string_view sv) { return std::string(sv); }

TEST_CASE("memstream put", "[text][memstream]")
{
	char buf[128];
	mh::memstream ms(buf);

	constexpr std::string_view TEST_STRING = "my test string";
	ms << TEST_STRING;

	REQUIRE(std::memcmp(buf, TEST_STRING.data(), TEST_STRING.size()) == 0);
	REQUIRE(ms.view() == TEST_STRING);
	CHECK(!ms.fail());
	CHECK(ms.good());
	REQUIRE(ms.tellp() == 14);
	REQUIRE(ms.tellg() == 0);
	REQUIRE(ms.good());

	ms.seekp(7);
	ms << " foo";

	constexpr std::string_view TEST_STRING_FOO = "my test fooing";
	REQUIRE(ms.view() == TEST_STRING_FOO);
	REQUIRE(std::memcmp(buf, TEST_STRING_FOO.data(), TEST_STRING_FOO.size()) == 0);

	{
		REQUIRE(ms.seekg(2));
		std::string testWord;
		REQUIRE(ms >> testWord);
		REQUIRE(ms.good());
		REQUIRE(testWord == "test");

		REQUIRE(ms >> testWord);
		REQUIRE(ms.eof());
		REQUIRE(testWord == "fooing");

		ms.clear(ms.rdstate() & ~std::ios_base::eofbit);
		REQUIRE(ms.good());

		REQUIRE(ms.seekp(0));
		REQUIRE(ms.good());

		REQUIRE(ms.write("foo", 3));
		REQUIRE(ms.good());
		REQUIRE(ms.view_full() == "footest fooing");
		REQUIRE(ms.view() == "");
		REQUIRE(ms.seekg(1));
		REQUIRE(ms.view() == "ootest fooing");

		REQUIRE(ms.seekg(0));
		REQUIRE(ms.good());
		REQUIRE(ms.view() == "footest fooing");

		REQUIRE(ms << "bar");
		REQUIRE(ms.view() == "foobart fooing");
		REQUIRE(ms.good());
	}

	{
		constexpr int TEST_INT_VALUE = 487;

		REQUIRE(ms.view() == "foobart fooing");
		REQUIRE(ms.seekp(1, std::ios::beg));
		REQUIRE(ms.seekp(5, std::ios::cur));
		REQUIRE(ms.tellp() == 6);

		REQUIRE(ms.seekg(0, std::ios::end));
		REQUIRE(ms.tellg() == 14);
		REQUIRE(ms.seekg(0));

		ms << TEST_INT_VALUE;
		CHECK(ms.tellp() == 9);
		CHECK(ms.tellg() == 0);
		CHECK(ms.seekg(0, std::ios::end));
		CHECK(ms.tellg() == 14);
		CHECK(ms.seekg(0));

		CHECK(ms.view() == "foobar487ooing");
		CHECK(ms.view_full() == "foobar487ooing");

		int testInt;
		REQUIRE(ms.seekg(6));
		ms >> testInt;
		REQUIRE(testInt == TEST_INT_VALUE);
	}
}

TEST_CASE("memstream buffer with existing data", "[text][memstream]")
{
	// A streambuf over a 16-byte buffer whose first 6 bytes already hold data:
	// - reads see exactly the existing data
	// - writes append after it, with the full remaining capacity available
	// - positions are absolute (relative to the start of the buffer)
	char buf[16] = { 'h', 'e', 'l', 'l', 'o', ' ' };
	mh::basic_memstreambuf<char> sb(buf, sizeof(buf), 6);

	REQUIRE(to_str(sb.view_full()) == "hello ");
	CHECK(static_cast<std::streamoff>(sb.pubseekoff(0, std::ios::cur, std::ios::out)) == 6);

	// xsputn always reserves one final slot, so 16 bytes with 6 used leaves 9
	const std::streamsize written = sb.sputn("0123456789", 10);
	CHECK(written == 9);
	REQUIRE(to_str(sb.view_full()) == "hello 012345678");
	CHECK(static_cast<std::streamoff>(sb.pubseekoff(0, std::ios::cur, std::ios::out)) == 15);

	// existing data must never exceed the buffer size
	REQUIRE_THROWS_AS(mh::basic_memstreambuf<char>(buf, 4, 5), std::invalid_argument);
}

TEST_CASE("memstream seek relative to end", "[text][memstream]")
{
	char buf[16];
	mh::memstream ms(buf, sizeof(buf));
	ms << "0123456789";
	REQUIRE(ms.good());

	SECTION("read side: negative offset from the end of the data")
	{
		REQUIRE(ms.seekg(-3, std::ios::end));
		CHECK(static_cast<std::streamoff>(ms.tellg()) == 7);

		std::string tail;
		REQUIRE(ms >> tail);
		CHECK(tail == "789");
	}
	SECTION("write side: negative offset from the end of the put area")
	{
		// the put area's end is the buffer's full capacity
		REQUIRE(ms.seekp(-8, std::ios::end));
		CHECK(static_cast<std::streamoff>(ms.tellp()) == 8);

		ms << "AB";
		REQUIRE(ms.good());
		REQUIRE(ms.seekg(0));
		CHECK(to_str(ms.view()) == "01234567AB");
	}
	SECTION("zero offset from the end")
	{
		REQUIRE(ms.seekg(0, std::ios::end));
		CHECK(static_cast<std::streamoff>(ms.tellg()) == 10);
	}
}

TEST_CASE("memstream write is payload-agnostic", "[text][memstream]")
{
	// no payload byte value may be mistaken for EOF and truncate the write
	char buf[8];
	mh::memstream ms(buf, sizeof(buf));

	constexpr char payload[3] = { 'A', '\xFF', 'B' };
	REQUIRE(ms.write(payload, 3));
	REQUIRE(ms.good());

	const auto view = ms.view_full();
	REQUIRE(view.size() == 3);
	CHECK(view[0] == 'A');
	CHECK(view[1] == '\xFF');
	CHECK(view[2] == 'B');
}

TEST_CASE("memstream full buffer: sputn writes nothing and returns 0", "[text][memstream]")
{
	char buf[4];
	mh::memstream ms(buf, sizeof(buf));

	// fill the buffer completely one char at a time (put() can use the final slot)
	ms.put('a');
	ms.put('b');
	ms.put('c');
	ms.put('d');
	REQUIRE(ms.good());
	REQUIRE(static_cast<std::streamoff>(ms.tellp()) == 4);
	REQUIRE(to_str(ms.view_full()) == "abcd");

	// sputn on the completely full buffer must write nothing and return 0;
	// in particular it must not move the put pointer backwards
	mh::basic_memstreambuf<char>& sb = ms;
	CHECK(sb.sputn("Z", 1) == 0);
	CHECK(static_cast<std::streamoff>(ms.tellp()) == 4);

	// a single-character write must also be rejected...
	CHECK(sb.sputc('X') == std::char_traits<char>::eof());

	// ...leaving the existing data intact
	CHECK(to_str(ms.view_full()) == "abcd");
}

TEST_CASE("memstream single-character writes are visible", "[text][memstream]")
{
	// put()/sputc bypass xsputn; their output must still show up in view(),
	// view_full() and reads
	char buf[8];
	mh::memstream ms(buf, sizeof(buf));

	ms.put('x');
	ms.put('y');
	REQUIRE(ms.good());
	CHECK(static_cast<std::streamoff>(ms.tellp()) == 2);
	CHECK(to_str(ms.view_full()) == "xy");
	CHECK(to_str(ms.view()) == "xy");

	std::string word;
	REQUIRE(ms >> word); // exercises underflow with a stale get area
	CHECK(word == "xy");
}

TEST_CASE("memstream does not log to stderr", "[text][memstream]")
{
	std::stringstream capture;
	std::streambuf* const oldCerrBuf = std::cerr.rdbuf(capture.rdbuf());

	{
		char buf[64];
		mh::memstream ms(buf, sizeof(buf));
		ms << "hello " << 42;
		ms.put('!');

		std::string word;
		ms >> word;
	}

	std::cerr.rdbuf(oldCerrBuf);
	CHECK(capture.str().empty());
}

TEST_CASE("memstream works for wchar_t", "[text][memstream]")
{
	wchar_t buf[32];
	mh::basic_memstream<wchar_t> ms(buf, 32);

	ms << L"wide " << 42;
	REQUIRE(ms.good());
	CHECK(std::wstring(ms.view()) == L"wide 42");

	std::wstring word;
	REQUIRE(ms >> word);
	CHECK(word == L"wide");
}

TEST_CASE("memstream is not default constructible", "[text][memstream]")
{
	// a default-constructed memstream would have no buffer to point at
	STATIC_REQUIRE(!std::is_default_constructible_v<mh::memstream>);
	STATIC_REQUIRE(!std::is_default_constructible_v<mh::basic_memstream<wchar_t>>);
}
