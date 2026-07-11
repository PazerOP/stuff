#include "mh/text/string_insertion.hpp"
#include <catch2/catch_all.hpp>

#include <string>

TEST_CASE("string insertion op", "[text][string_insertion]")
{
	std::string test;
	test << "Hello" << " world" << " !";
	REQUIRE(test == "Hello world !");
}

TEST_CASE("string insertion op - rvalue string", "[text][string_insertion]")
{
	// inserting into a temporary string must compile and evaluate to the
	// written-to string
	const std::string result = (std::string("x") << 5);
	REQUIRE(result == "x5");

	const std::string chained = (std::string("val: ") << 1 << ',' << 2);
	REQUIRE(chained == "val: 1,2");

	const std::string boolStr = (std::string() << true);
	REQUIRE(boolStr == "true"); // bool inserts as boolalpha
}
