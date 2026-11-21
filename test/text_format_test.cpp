#include "mh/text/format.hpp"
#include "mh/text/formatters/error_code.hpp"
#include "mh/source_location.hpp"
#include <catch2/catch_all.hpp>

#include <cerrno>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

TEST_CASE("format - a known formatter backend is selected", "[text][format]")
{
	STATIC_REQUIRE(MH_FORMATTER == MH_FORMATTER_NONE
		|| MH_FORMATTER == MH_FORMATTER_FMTLIB
		|| MH_FORMATTER == MH_FORMATTER_STL);
}

#if MH_FORMATTER != MH_FORMATTER_NONE

TEST_CASE("format_to and friends accept runtime format strings", "[text][format]")
{
	// The format strings reaching these helpers are runtime values; they must
	// be routed around the backend's compile-time format string checks
	std::string out;
	mh::format_to(std::back_inserter(out), "{}", 42);
	REQUIRE(out == "42");

	const std::string_view runtimeFmtStr = "{}-{}";
	out.clear();
	mh::format_to(std::back_inserter(out), runtimeFmtStr, 1, 2);
	REQUIRE(out == "1-2");

	char buf[4] = {};
	const auto result = mh::format_to_n(buf, sizeof(buf), "{}", 123456);
	CHECK(std::string(buf, sizeof(buf)) == "1234"); // truncated to n
	CHECK(result.size == 6);                        // untruncated size

	std::string container = "have ";
	mh::format_to_container(container, "{} {}", 2, "cows");
	REQUIRE(container == "have 2 cows");
}

TEST_CASE("build_string", "[text][format]")
{
	REQUIRE(mh::build_string(1, 'x') == "1x");
	REQUIRE(mh::build_string("a", 2, "c") == "a2c");
}

TEST_CASE("try_format returns an error string instead of throwing", "[text][format]")
{
	REQUIRE(mh::try_format("{} {}", 1, 2) == "1 2");

	const auto bad = mh::try_format("{:bogus}", 42);
	REQUIRE_THAT(bad, Catch::Matchers::StartsWith("FORMATTING ERROR:"));
	REQUIRE_THAT(bad, Catch::Matchers::ContainsSubstring("{:bogus}"));
}

TEST_CASE("try_vformat - char", "[text][format]")
{
	int value = 42;
	REQUIRE(mh::try_vformat("{}", mh::make_format_args(value)) == "42");

	const auto bad = mh::try_vformat("{:bogus}", mh::make_format_args(value));
	REQUIRE_THAT(bad, Catch::Matchers::StartsWith("FORMATTING ERROR:"));
	REQUIRE_THAT(bad, Catch::Matchers::ContainsSubstring("{:bogus}"));
}

#if MH_FORMATTER == MH_FORMATTER_FMTLIB
TEST_CASE("try_vformat - wchar_t", "[text][format]")
{
	// fmt's wide vformat overload only deduces against fmt::wstring_view
	int value = 42;
	const std::wstring good = mh::try_vformat(fmt::wstring_view(L"{}"), fmt::make_wformat_args(value));
	REQUIRE(good == L"42");

	const std::wstring bad = mh::try_vformat(fmt::wstring_view(L"{:bogus}"), fmt::make_wformat_args(value));
	REQUIRE(bad.find(L"FORMATTING ERROR:") == 0);
	REQUIRE(bad.find(L"{:bogus}") != std::wstring::npos);
}
#endif // MH_FORMATTER == MH_FORMATTER_FMTLIB

namespace
{
	// fmt >= 12 (and std::format) require formatter::format() to be callable
	// on a const formatter object
	template<typename TValue>
	constexpr bool formatter_is_const_callable = requires(
		const mh::formatter<TValue, char>& constFormatter,
		const TValue& value,
		mh::format_context& ctx)
	{
		constFormatter.format(value, ctx);
	};
}

TEST_CASE("bundled formatters are const-callable", "[text][format]")
{
	STATIC_REQUIRE(formatter_is_const_callable<std::error_code>);
	STATIC_REQUIRE(formatter_is_const_callable<std::error_condition>);
	STATIC_REQUIRE(formatter_is_const_callable<mh::source_location>);
}

TEST_CASE("error_code formatter", "[text][format]")
{
	const std::error_code ec(ENOENT, std::generic_category());

	// default presentation: category(value): message
	const auto formatted = mh::format("{}", ec);
	REQUIRE(formatted == mh::format("{}({}): {}", ec.category().name(), ec.value(), ec.message()));

	// individual presentation flags
	CHECK(mh::format("{:c}", ec) == ec.category().name());
	CHECK(mh::format("{:v}", ec) == mh::format("({})", ec.value()));
	CHECK(mh::format("{:m}", ec) == ec.message());
	CHECK(mh::format("{:cvm}", ec) == formatted);

	const std::error_condition cond = ec.default_error_condition();
	CHECK_FALSE(mh::format("{}", cond).empty());
}

TEST_CASE("error_code formatter rejects invalid specs", "[text][format]")
{
	const std::error_code ec(ENOENT, std::generic_category());

	// unknown presentation character
	CHECK_THROWS_AS(mh::format("{:z}", ec), mh::format_error);

	// unterminated format spec
	CHECK_THROWS_AS(mh::format("{:m", ec), mh::format_error);
}

namespace
{
	static std::string short_file_name(const mh::source_location& loc)
	{
		const std::string_view file = loc.file_name();
		const auto lastSlash = file.find_last_of("/\\");
		return std::string(lastSlash == file.npos ? file : file.substr(lastSlash + 1));
	}
}

TEST_CASE("source_location formatter", "[text][format]")
{
	const auto loc = MH_SOURCE_LOCATION_CURRENT();

	// default presentation: short path, line, function
	const auto formatted = mh::format("{}", loc);
	REQUIRE(formatted == mh::format("{}({}):{}", short_file_name(loc), loc.line(), loc.function_name()));

	// individual presentation flags
	CHECK(mh::format("{:l}", loc) == mh::format("({})", loc.line()));
	CHECK(mh::format("{:f}", loc) == loc.function_name());
	CHECK(mh::format("{:p}", loc) == short_file_name(loc));
	CHECK(mh::format("{:P}", loc) == loc.file_name());
}

// defined in text_format_link_tu.cpp
std::string format_source_location_from_second_tu();

TEST_CASE("source_location formatting links across translation units", "[text][format]")
{
	// two translation units both formatting a source_location must link in
	// header-only mode as well as in library mode
	const auto fromSecondTu = format_source_location_from_second_tu();
	REQUIRE_THAT(fromSecondTu, Catch::Matchers::ContainsSubstring("text_format_link_tu.cpp"));

	const auto fromThisTu = mh::format("{}", MH_SOURCE_LOCATION_CURRENT());
	REQUIRE_THAT(fromThisTu, Catch::Matchers::ContainsSubstring("text_format_test.cpp"));
}

#endif // MH_FORMATTER != MH_FORMATTER_NONE
