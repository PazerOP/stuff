#include "mh/memory/buffer.hpp"
#include "catch2/repo/single_include/catch2/catch.hpp"

#include <cstring>

TEST_CASE("buffer - common", "[memory][buffer]")
{
	mh::buffer buf;
	buf.resize(48);
	REQUIRE(buf.size() == 48);

	SECTION("Grow")
	{
		buf.resize(891);
		REQUIRE(buf.size() == 891);
	}
	SECTION("Clear")
	{
		buf.clear();
		REQUIRE(buf.data() == nullptr);
		REQUIRE(buf.size() == 0);
	}
}

TEST_CASE("buffer - reserve", "[memory][buffer]")
{
	mh::buffer buf;
	REQUIRE(buf.reserve(16));
	const auto postReserveSize = buf.size();
	REQUIRE(postReserveSize >= 16);
	REQUIRE(buf.size() == postReserveSize);

	REQUIRE(!buf.reserve(16));
	REQUIRE(buf.size() == postReserveSize);

	REQUIRE(!buf.reserve(1));
	REQUIRE(buf.size() == postReserveSize);

	buf.resize(1);
	REQUIRE(buf.size() == 1);
}

TEST_CASE("buffer - resize preserves data", "[memory][buffer]")
{
	mh::buffer buf;
	constexpr const char TEST_STR[] = "don't delete me :(";
	buf.resize(sizeof(TEST_STR) * 25);
	std::memcpy((char*)buf.data(), TEST_STR, sizeof(TEST_STR));

	SECTION("Grow")
	{
		buf.resize(sizeof(TEST_STR) * 50);
	}
	SECTION("Shrink")
	{
		buf.resize(sizeof(TEST_STR));
	}

	REQUIRE(!std::memcmp(buf.data(), TEST_STR, sizeof(TEST_STR)));
}

TEST_CASE("buffer - constructor - default", "[memory][buffer]")
{
	mh::buffer buf;
	REQUIRE(buf.data() == nullptr);
	REQUIRE(buf.size() == 0);
}

TEST_CASE("buffer - constructor - initial size", "[memory][buffer]")
{
	constexpr size_t TEST_SIZE = 4892;
	mh::buffer buf(TEST_SIZE);
	REQUIRE(buf.data() != nullptr);
	REQUIRE(buf.size() == TEST_SIZE);

	// Make sure we can write to all the bytes
	std::memset(buf.data(), 0x42, TEST_SIZE);
}

TEST_CASE("buffer - constructor - initial data", "[memory][buffer]")
{
	constexpr const char TEST_DATA[] = "very cool test framework";
	mh::buffer buf((const std::byte*)TEST_DATA, sizeof(TEST_DATA));
	REQUIRE(buf.size() == sizeof(TEST_DATA));
	REQUIRE(!std::memcmp(buf.data(), TEST_DATA, sizeof(TEST_DATA)));
}

TEST_CASE("buffer - constructor - copy constructor", "[memory][buffer]")
{
	constexpr const char TEST_DATA[] = "very cool test framework";
	mh::buffer buf((const std::byte*)TEST_DATA, sizeof(TEST_DATA));
	REQUIRE(buf.size() == sizeof(TEST_DATA));
	REQUIRE(!std::memcmp(buf.data(), TEST_DATA, sizeof(TEST_DATA)));
}
