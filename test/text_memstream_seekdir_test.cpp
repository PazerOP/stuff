// The "unknown seekdir" branch of basic_memstreambuf::seekoff runs
// assert(false) before its throw, so it is only reachable with NDEBUG.
// This TU defines NDEBUG *before* including the header, and instantiates the
// template with a TU-local traits type so the assert-free instantiation cannot
// collide (ODR) with the assert-carrying instantiations in the other test TUs.
#ifndef NDEBUG
#define NDEBUG 1
#endif

#include "mh/text/memstream.hpp"
#include <catch2/catch_all.hpp>

#include <stdexcept>
#include <string>

namespace
{
	struct local_char_traits : std::char_traits<char> {};
}

TEST_CASE("memstream seekoff rejects an unknown seekdir", "[text][memstream]")
{
	char buf[8];
	mh::basic_memstreambuf<char, local_char_traits> sb(buf, sizeof(buf));

	// 3 is not beg/cur/end on any implementation (and stays inside the enum's
	// value range, unlike e.g. ~0); it must be reported, not silently mis-seek
	const auto badDir = static_cast<std::ios_base::seekdir>(3);
	CHECK_THROWS_AS(sb.pubseekoff(0, badDir, std::ios::in), std::invalid_argument);
}
