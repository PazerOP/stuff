#include "mh/text/stringops.hpp"
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

TEST_CASE("trim - empty string", "[text][stringops]")
{
	std::string str;

	SECTION("trim_start")
	{
		str = mh::trim_start(std::move(str));
		REQUIRE(str.empty());
	}
	SECTION("trim_end")
	{
		str = mh::trim_end(std::move(str));
		REQUIRE(str.empty());
	}
	SECTION("trim")
	{
		str = mh::trim(std::move(str));
		REQUIRE(str.empty());
	}
}

TEST_CASE("trim - non-empty string", "[text][stringops]")
{
	std::string str = " hello.........\r.\n \t";

	SECTION("trim_start")
	{
		str = mh::trim_start(std::move(str));
		REQUIRE(str == "hello.........\r.\n \t");
	}
	SECTION("trim_end")
	{
		str = mh::trim_end(std::move(str));
		REQUIRE(str == " hello.........\r.");
	}
	SECTION("trim")
	{
		str = mh::trim(std::move(str));
		REQUIRE(str == "hello.........\r.");
	}
}
