#include <catch2/catch_all.hpp>

#ifdef __cpp_lib_to_chars
#include "mh/text/charconv_helper.hpp"
#include "mh/text/string_insertion.hpp"
#include "last_include.hpp"

TEST_CASE("charconv helpers", "[text][charconv_helper]")
{
	std::string test;
	test << "Hello" << " world" << " !";
	REQUIRE(test == "Hello world !");
}
#endif
