#include "mh/error/not_implemented_error.hpp"

#include <catch2/catch_all.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

TEST_CASE("not_implemented_error - is a logic_error", "[error][not_implemented_error]")
{
	STATIC_CHECK(std::is_base_of_v<std::logic_error, mh::not_implemented_error>);

	// Pass the location explicitly: on toolchains without std::source_location
	// (e.g. clang-14/libc++-14), MH_SOURCE_LOCATION_AUTO degrades to a
	// parameter with no default argument.
	CHECK_THROWS_AS(throw mh::not_implemented_error(MH_SOURCE_LOCATION_CURRENT()), std::logic_error);
}

TEST_CASE("not_implemented_error - what() describes the call site", "[error][not_implemented_error]")
{
	const auto location = MH_SOURCE_LOCATION_CURRENT();
	const mh::not_implemented_error err(location);

	const std::string_view what = err.what();
	CHECK(what.find(location.file_name()) != std::string_view::npos);
	CHECK(what.find("(" + std::to_string(+location.line()) + "):") != std::string_view::npos);
	CHECK(what.find(location.function_name()) != std::string_view::npos);
	CHECK(what.find(" not implemented") != std::string_view::npos);
}

TEST_CASE("not_implemented_error - location() reports the construction site", "[error][not_implemented_error]")
{
	const auto location = MH_SOURCE_LOCATION_CURRENT();
	const mh::not_implemented_error err(location);

	CHECK(err.location().line() == location.line());
	CHECK(std::string_view(err.location().file_name()) == location.file_name());
	CHECK(std::string_view(err.location().function_name()) == location.function_name());
}
