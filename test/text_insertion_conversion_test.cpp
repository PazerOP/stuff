#include "mh/text/insertion_conversion.hpp"
#include <catch2/catch_all.hpp>

#include <sstream>
#include <string>
#include <string_view>

TEST_CASE("insertion_conversion - wide string into narrow ostream", "[text][insertion_conversion]")
{
	// the mismatched-character-type string insertion must actually insert the
	// characters (C++20 deletes the wchar_t* overload this used to rely on)
	std::ostringstream os;
	os << std::wstring(L"wide");
	REQUIRE(os.str() == "wide");
}

TEST_CASE("insertion_conversion - wide string_view into narrow ostream", "[text][insertion_conversion]")
{
	std::ostringstream os;
	os << std::wstring_view(L"view");
	REQUIRE(os.str() == "view");
}

TEST_CASE("insertion_conversion - char16_t string into narrow ostream", "[text][insertion_conversion]")
{
	std::ostringstream os;
	os << std::u16string(u"sixteen");
	REQUIRE(os.str() == "sixteen");
}

TEST_CASE("insertion_conversion - narrow string into wide ostream", "[text][insertion_conversion]")
{
	std::wostringstream os;
	os << std::string("narrow");
	REQUIRE(os.str() == L"narrow");
}
