#include "mh/text/memstream.hpp"
#include <catch2/catch_all.hpp>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

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
	REQUIRE(to_str(ms.view()) == to_str(TEST_STRING));
	CHECK(!ms.fail());
	CHECK(ms.good());
	REQUIRE(ms.tellp() == 14);
	REQUIRE(ms.tellg() == 0);
	REQUIRE(ms.good());

	ms.seekp(7);
	ms << " foo";

	constexpr std::string_view TEST_STRING_FOO = "my test fooing";
	REQUIRE(to_str(ms.view()) == to_str(TEST_STRING_FOO));
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
		REQUIRE(to_str(ms.view_full()) == "footest fooing");
		REQUIRE(to_str(ms.view()) == "");
		REQUIRE(ms.seekg(1));
		REQUIRE(to_str(ms.view()) == "ootest fooing");

		REQUIRE(ms.seekg(0));
		REQUIRE(ms.good());
		REQUIRE(to_str(ms.view()) == "footest fooing");

		REQUIRE(ms << "bar");
		REQUIRE(to_str(ms.view()) == "foobart fooing");
		REQUIRE(ms.good());
	}

	{
		constexpr int TEST_INT_VALUE = 487;

		REQUIRE(to_str(ms.view()) == "foobart fooing");
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

		CHECK(to_str(ms.view()) == "foobar487ooing");
		CHECK(to_str(ms.view_full()) == "foobar487ooing");

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

TEST_CASE("memstream seek with nonzero offset from current position", "[text][memstream]")
{
	char buf[16];
	mh::memstream ms(buf, sizeof(buf));
	ms << "0123456789";

	REQUIRE(ms.seekg(1));
	REQUIRE(ms.seekg(2, std::ios::cur)); // read side, relative
	CHECK(static_cast<std::streamoff>(ms.tellg()) == 3);

	char c;
	REQUIRE(ms.get(c));
	CHECK(c == '3');
}

TEST_CASE("memstream seekoff rejects an empty openmode", "[text][memstream]")
{
	char buf[8];
	mh::memstream ms(buf, sizeof(buf));

	// neither ios::in nor ios::out: there is nothing to seek
	CHECK_THROWS_AS(ms.rdbuf()->pubseekoff(0, std::ios::cur, std::ios_base::openmode{}),
		std::invalid_argument);
}

TEST_CASE("memstream setbuf replaces the buffer", "[text][memstream]")
{
	char first[8];
	mh::memstream ms(first, sizeof(first));
	ms << "aa";

	char second[8] = {};
	REQUIRE(ms.rdbuf()->pubsetbuf(second, sizeof(second)) == ms.rdbuf());

	// writes now target the new buffer, positions reset to its start
	ms << "zz";
	CHECK(second[0] == 'z');
	CHECK(second[1] == 'z');
	CHECK(to_str(ms.view_full()) == "zz");
}

TEST_CASE("memstream wide: seeks, reads and writes", "[text][memstream]")
{
	wchar_t buf[16];
	mh::basic_memstream<wchar_t> ms(buf, 16);

	ms << L"0123456789";
	REQUIRE(ms.good());
	CHECK(std::wstring(ms.view_full()) == L"0123456789");

	// tellg/tellp (seekoff fast path: cur with zero offset)
	CHECK(static_cast<std::streamoff>(ms.tellg()) == 0);
	CHECK(static_cast<std::streamoff>(ms.tellp()) == 10);

	// absolute seek + read to the end of the data (underflow -> eof)
	REQUIRE(ms.seekg(2));
	std::wstring word;
	REQUIRE(ms >> word);
	CHECK(word == L"23456789");
	CHECK(ms.eof());
	ms.clear();

	// relative seeks with nonzero offsets
	REQUIRE(ms.seekg(0));
	REQUIRE(ms.seekg(3, std::ios::cur));
	CHECK(static_cast<std::streamoff>(ms.tellg()) == 3);

	REQUIRE(ms.seekp(0));
	REQUIRE(ms.seekp(4, std::ios::cur));
	CHECK(static_cast<std::streamoff>(ms.tellp()) == 4);

	// seeks relative to the end
	REQUIRE(ms.seekg(-2, std::ios::end));
	CHECK(static_cast<std::streamoff>(ms.tellg()) == 8);

	REQUIRE(ms.seekp(-8, std::ios::end)); // the put area's end is the full capacity
	CHECK(static_cast<std::streamoff>(ms.tellp()) == 8);

	ms << L"AB";
	REQUIRE(ms.good());
	REQUIRE(ms.seekg(0));
	CHECK(std::wstring(ms.view()) == L"01234567AB");
}

TEST_CASE("memstream wide: pubseekpos with in|out moves both positions", "[text][memstream]")
{
	wchar_t buf[16];
	mh::basic_memstream<wchar_t> ms(buf, 16);
	ms << L"0123456789";

	const auto pos = ms.rdbuf()->pubseekpos(5, std::ios::in | std::ios::out);
	CHECK(static_cast<std::streamoff>(pos) == 5);
	CHECK(static_cast<std::streamoff>(ms.tellg()) == 5);
	CHECK(static_cast<std::streamoff>(ms.tellp()) == 5);
}

TEST_CASE("memstream wide: full buffer", "[text][memstream]")
{
	wchar_t buf[4];
	mh::basic_memstream<wchar_t> ms(buf, 4);
	mh::basic_memstreambuf<wchar_t>& sb = ms;

	// xsputn always reserves one final slot
	CHECK(sb.sputn(L"abcdef", 6) == 3);
	CHECK(std::wstring(ms.view_full()) == L"abc");

	// ...which is reachable one character at a time
	CHECK(sb.sputc(L'd') != std::char_traits<wchar_t>::eof());
	CHECK(std::wstring(ms.view_full()) == L"abcd");

	// writing past the end reports eof via overflow, existing data is intact
	CHECK(sb.sputc(L'X') == std::char_traits<wchar_t>::eof());
	CHECK(sb.sputn(L"Y", 1) == 0);
	CHECK(std::wstring(ms.view_full()) == L"abcd");
}

TEST_CASE("memstream wide: single-character writes are visible", "[text][memstream]")
{
	wchar_t buf[8];
	mh::basic_memstream<wchar_t> ms(buf, 8);

	ms.put(L'x');
	ms.put(L'y');
	REQUIRE(ms.good());

	std::wstring word;
	REQUIRE(ms >> word); // exercises underflow with a stale get area
	CHECK(word == L"xy");
}

TEST_CASE("memstream wide: setbuf replaces the buffer", "[text][memstream]")
{
	wchar_t first[8];
	mh::basic_memstream<wchar_t> ms(first, 8);
	ms << L"aa";

	wchar_t second[8] = {};
	REQUIRE(ms.rdbuf()->pubsetbuf(second, 8) == ms.rdbuf());

	ms << L"zz";
	CHECK(second[0] == L'z');
	CHECK(second[1] == L'z');
	CHECK(std::wstring(ms.view_full()) == L"zz");
}

namespace
{
	// exposes the protected virtual for direct testing
	template<typename CharT>
	struct exposed_memstreambuf final : mh::basic_memstreambuf<CharT>
	{
		using mh::basic_memstreambuf<CharT>::basic_memstreambuf;
		using mh::basic_memstreambuf<CharT>::overflow;
	};
}

TEST_CASE("memstream overflow stores a character when there is room", "[text][memstream]")
{
	using traits = std::char_traits<wchar_t>;

	wchar_t buf[4];
	exposed_memstreambuf<wchar_t> sb(buf, 4);

	// called with eof, overflow is a no-op query
	CHECK(sb.overflow() == traits::eof());

	// called with a character while there is room, overflow stores it
	CHECK(sb.overflow(traits::to_int_type(L'Q')) == traits::to_int_type(L'Q'));
	CHECK(std::wstring(sb.view_full()) == L"Q");
}

TEST_CASE("memstream is not default constructible", "[text][memstream]")
{
	// a default-constructed memstream would have no buffer to point at
	STATIC_REQUIRE(!std::is_default_constructible_v<mh::memstream>);
	STATIC_REQUIRE(!std::is_default_constructible_v<mh::basic_memstream<wchar_t>>);
}
