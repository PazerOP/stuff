#include "mh/memory/buffer.hpp"
#include <catch2/catch_all.hpp>

#include <compare>
#include <cstring>
#include "last_include.hpp"

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
	mh::buffer src((const std::byte*)TEST_DATA, sizeof(TEST_DATA));
	mh::buffer buf(src);
	REQUIRE(buf.size() == sizeof(TEST_DATA));
	REQUIRE(buf.data() != src.data());
	REQUIRE(!std::memcmp(buf.data(), TEST_DATA, sizeof(TEST_DATA)));

	mh::buffer emptySrc;
	mh::buffer emptyCopy(emptySrc);
	REQUIRE(emptyCopy.size() == 0);
}

TEST_CASE("buffer - resize to zero", "[memory][buffer]")
{
	mh::buffer buf(8);
	REQUIRE(buf.size() == 8);

	// resizing to zero is a legitimate request: it must empty the buffer, not throw
	REQUIRE_NOTHROW(buf.resize(0));
	REQUIRE(buf.size() == 0);
	REQUIRE(buf.data() == nullptr);

	SECTION("grow again after shrinking to zero")
	{
		buf.resize(16);
		REQUIRE(buf.size() == 16);
		REQUIRE(buf.data() != nullptr);
		std::memset(buf.data(), 0x42, buf.size());

		buf.resize(0);
		REQUIRE(buf.size() == 0);
		REQUIRE(buf.data() == nullptr);
	}
	// destruction after resize(0) must be safe (checked at end of scope)
}

TEST_CASE("buffer - constructor - zero size", "[memory][buffer]")
{
	mh::buffer buf(size_t(0));
	REQUIRE(buf.size() == 0);
	REQUIRE(buf.data() == nullptr);
}

TEST_CASE("buffer - empty buffer copies", "[memory][buffer]")
{
	mh::buffer empty;
	mh::buffer alsoEmpty(empty);
	REQUIRE(alsoEmpty.size() == 0);
	REQUIRE(alsoEmpty.data() == nullptr);
}

// mirrors the feature guard around buffer's operator<=>
#if ((__cpp_lib_three_way_comparison >= 201907) || defined(_MSC_VER)) && (__cpp_impl_three_way_comparison >= 201907)
TEST_CASE("buffer - three-way comparison", "[memory][buffer]")
{
	SECTION("empty <=> empty")
	{
		const mh::buffer empty;
		const mh::buffer alsoEmpty;
		REQUIRE(((empty <=> alsoEmpty) == std::strong_ordering::equal));
	}
	SECTION("empty <=> non-empty")
	{
		const mh::buffer empty;
		const mh::buffer nonEmpty((const std::byte*)"x", 1);
		REQUIRE(((empty <=> nonEmpty) == std::strong_ordering::less));
		REQUIRE(((nonEmpty <=> empty) == std::strong_ordering::greater));
	}
	SECTION("contents")
	{
		const mh::buffer ab((const std::byte*)"ab", 2);
		const mh::buffer alsoAb((const std::byte*)"ab", 2);
		const mh::buffer ac((const std::byte*)"ac", 2);

		REQUIRE(((ab <=> alsoAb) == std::strong_ordering::equal));
		REQUIRE(((ab <=> ac) == std::strong_ordering::less));
		REQUIRE(((ac <=> ab) == std::strong_ordering::greater));
	}
}
#endif
