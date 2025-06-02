#include "mh/algorithm/algorithm.hpp"
#include <catch2/catch_all.hpp>

#include <string>
#include <vector>
#include "last_include.hpp"

TEST_CASE("find_or_add empty vector", "[algorithm]")
{
	std::vector<std::string> vec;

	SECTION("Empty vector find and add")
	{
		auto result = mh::find_or_add(vec, "hello");
		REQUIRE(result.first == true);
		REQUIRE(result.second == vec.begin());
		REQUIRE(*result.second == "hello");
		REQUIRE(vec.size() == 1);
		REQUIRE(vec.at(0) == "hello");
	}
}

TEST_CASE("find_or_add populated vector", "[algorithm]")
{
	std::vector<std::string> vec;
	vec.push_back("my cool string");
	vec.push_back("this one's pretty cool too");

	SECTION("Existing vector find")
	{
		auto result = mh::find_or_add(vec, "my cool string");
		REQUIRE(result.first == false);
		REQUIRE(vec.size() == 2);
		REQUIRE(*result.second == "my cool string");
	}
	SECTION("Existing vector add")
	{
		auto result = mh::find_or_add(vec, "my even cooler string");
		REQUIRE(result.first == true);
		REQUIRE(vec.size() == 3);
		REQUIRE(*result.second == "my even cooler string");
	}
}
