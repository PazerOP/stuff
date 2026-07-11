#include "mh/variant.hpp"
#include <catch2/catch_all.hpp>

#include <cstddef>
#include <variant>

TEST_CASE("variant_type_index - finds every alternative position", "[variant]")
{
	using test_variant = std::variant<int, float, char>;

	// first, middle, and last alternatives must all resolve
	STATIC_REQUIRE(mh::variant_type_index<int>(test_variant{}) == 0);
	STATIC_REQUIRE(mh::variant_type_index<float>(test_variant{}) == 1);
	STATIC_REQUIRE(mh::variant_type_index<char>(test_variant{}) == 2);

	// a type that is not an alternative reports "not found"
	STATIC_REQUIRE(mh::variant_type_index<double>(test_variant{}) == static_cast<size_t>(-1));

	// single-alternative variant
	STATIC_REQUIRE(mh::variant_type_index<long>(std::variant<long>{}) == 0);

	SUCCEED("all lookups resolved at compile time");
}

TEST_CASE("variant_type_index_v - finds every alternative position", "[variant]")
{
	using test_variant = std::variant<int, float, char>;

	STATIC_REQUIRE(mh::variant_type_index_v<test_variant, int> == 0);
	STATIC_REQUIRE(mh::variant_type_index_v<test_variant, float> == 1);
	STATIC_REQUIRE(mh::variant_type_index_v<test_variant, char> == 2);

	SUCCEED("all lookups resolved at compile time");
}
