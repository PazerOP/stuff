#include "mh/source_location.hpp"
#include <catch2/catch_all.hpp>

#include <sstream>
#include <string>

// When the standard library provides std::source_location, mh::source_location
// is an alias for it and the inserter in namespace mh is NOT found by ADL (the
// argument's associated namespace is std). Callers have to import it.
using mh::operator<<;

TEST_CASE("source_location - ostream insertion", "[source_location]")
{
	const mh::source_location loc = MH_SOURCE_LOCATION_CURRENT();
	const auto expectedLine = loc.line();

	std::ostringstream oss;
	oss << loc;
	const std::string text = oss.str();

	CHECK(text.find("source_location_test.cpp") != std::string::npos);
	CHECK(text.find("(" + std::to_string(expectedLine) + "):") != std::string::npos);
	CHECK(text.find(loc.function_name()) != std::string::npos);

	// the inserter is CharT-generic; the wide instantiation must work too
	std::wostringstream woss;
	woss << loc;
	CHECK(woss.str().find(L"source_location_test.cpp") != std::wstring::npos);
}

#if MH_FORMATTER != MH_FORMATTER_NONE

TEST_CASE("source_location - format presentations", "[source_location][format]")
{
	const mh::source_location loc = MH_SOURCE_LOCATION_CURRENT();
	const std::string line = std::to_string(loc.line());
	const std::string function = loc.function_name();

	// default: short path + line + function
	const std::string def = mh::format("{}", loc);
	CHECK(def == "source_location_test.cpp(" + line + "):" + function);

	// an explicit spec selects exactly what was asked for
	CHECK(mh::format("{:plf}", loc) == def);
	CHECK(mh::format("{:p}", loc) == "source_location_test.cpp");
	CHECK(mh::format("{:l}", loc) == "(" + line + ")");
	CHECK(mh::format("{:f}", loc) == function);
	CHECK(mh::format("{:pf}", loc) == "source_location_test.cpp:" + function);
	CHECK(mh::format("{:pl}", loc) == "source_location_test.cpp(" + line + ")");

	// full path: same tail, but keeps the directories
	const std::string full = mh::format("{:P}", loc);
	CHECK(full.find("source_location_test.cpp") != std::string::npos);
	CHECK(full == loc.file_name());
}

TEST_CASE("source_location - invalid format specs", "[source_location][format]")
{
	const mh::source_location loc = MH_SOURCE_LOCATION_CURRENT();

	// 'p' and 'P' are mutually exclusive
	CHECK_THROWS_AS(mh::format("{:pP}", loc), mh::format_error);
	CHECK_THROWS_AS(mh::format("{:Pp}", loc), mh::format_error);

	// unknown presentation character
	CHECK_THROWS_AS(mh::format("{:z}", loc), mh::format_error);

	// unterminated format spec
	CHECK_THROWS_AS(mh::format("{:p", loc), mh::format_error);
}

#endif // MH_FORMATTER != MH_FORMATTER_NONE
