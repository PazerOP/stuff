#include "mh/io/getopt.hpp"

#if __has_include(<getopt.h>)
#include <catch2/catch_all.hpp>

#include <array>
#include <iterator>
#include <string>
#include <string.h>
#include "last_include.hpp"
#include <utility>
#include <vector>

TEST_CASE("getopt")
{
	static const option options[] =
	{
		{ "file", required_argument, nullptr, 'f' },
		{ "directory", required_argument, nullptr, 'C' },
		{ "strip-components", required_argument, nullptr, 0 },
		{ "verbose", no_argument, nullptr, 'v' },
		{},
	};

	char* const argv[] =
	{
		strdup("tar"),
		strdup("-xf"),
		strdup("test_tar.tar.gz"),
		strdup("-C"),
		strdup("test_extract_dir/something/yes"),
		strdup("--strip-components"),
		strdup("1"),
		strdup("--verbose"),
		nullptr
	};

	constexpr int argc = std::size(argv) - 1;
	constexpr int actual_arg_count = 5;

	const std::array<mh::parsed_option, actual_arg_count> expected_options =
	{
		mh::parsed_option('x', {}, false, options, -1),
		mh::parsed_option('f', "test_tar.tar.gz", false, options, -1),
		mh::parsed_option('C', "test_extract_dir/something/yes", false, options, -1),
		mh::parsed_option(0, "1", false, options, 2),
		mh::parsed_option('v', {}, false, options, 3),
	};
	constexpr std::array<std::string_view, actual_arg_count> expected_arg_name_long =
	{
		std::string_view(),
		std::string_view(),
		std::string_view(),
		"strip-components",
		"verbose"
	};
	constexpr std::array<char, actual_arg_count> expected_arg_name_short = { 'x', 'f', 'C', 0, 'v' };

	int index = 0;
	const bool result = mh::parse_args(argc, argv, "C:xf:v", options, [&](const mh::parsed_option& opt)
	{
		const mh::parsed_option& expected = expected_options.at(index);
		CAPTURE(index, opt, expected);

		REQUIRE(opt.getopt_result == expected.getopt_result);
		REQUIRE(opt.arg_value == expected.arg_value);
		REQUIRE(opt.is_non_option == expected.is_non_option);
		REQUIRE(opt.get_longopt() == expected.get_longopt());
		REQUIRE(opt.get_longopt_safe() == expected.get_longopt_safe());
		REQUIRE(opt.longopt_index == expected.longopt_index);

		REQUIRE_THAT(opt.get_arg_name(), Catch::Matchers::Equals(expected.get_arg_name()));

		REQUIRE(opt.get_arg_name_long() == expected_arg_name_long.at(index));
		REQUIRE(expected.get_arg_name_long() == expected_arg_name_long.at(index)); // Sanity check
		REQUIRE(opt.get_arg_name_short() == expected_arg_name_short.at(index));

		index++;
		return true;
	});

	REQUIRE(result == true);
	REQUIRE(index == actual_arg_count);

	for (char* arg : argv)
		free(arg);
}

TEST_CASE("getopt - option equality", "[io][getopt]")
{
	// regression: under C++20 the header only provided a free operator<=>, and
	// operator== is not synthesized from a non-defaulted <=>, so option == option
	// failed to compile in the exact language mode the library targets
	constexpr option a{ "file", required_argument, nullptr, 'f' };
	constexpr option b{ "file", required_argument, nullptr, 'f' };
	constexpr option c{ "directory", no_argument, nullptr, 'C' };

	REQUIRE(a == b);
	REQUIRE_FALSE(a == c);
	REQUIRE(a != c);
	REQUIRE_FALSE(a != b);
}

TEST_CASE("getopt - non-option arguments are passed to the callback", "[io][getopt]")
{
	char* const argv[] =
	{
		strdup("prog"),
		strdup("-v"),
		strdup("positional1"),
		strdup("positional2"),
		nullptr
	};
	constexpr int argc = std::size(argv) - 1;

	std::vector<std::pair<std::string, bool>> seen;
	const bool result = mh::parse_args(argc, argv, "v", [&](const mh::parsed_option& opt)
	{
		if (opt.is_non_option)
			seen.emplace_back(std::string(opt.arg_value), true);
		else
			seen.emplace_back(std::string(1, char(opt.getopt_result)), false);

		return true;
	});

	REQUIRE(result == true);
	REQUIRE(seen.size() == 3);
	CHECK(seen.at(0) == std::pair<std::string, bool>("v", false));
	CHECK(seen.at(1) == std::pair<std::string, bool>("positional1", true));
	CHECK(seen.at(2) == std::pair<std::string, bool>("positional2", true));

	for (char* arg : argv)
		free(arg);
}

TEST_CASE("getopt - callback can reject non-option arguments", "[io][getopt]")
{
	char* const argv[] =
	{
		strdup("prog"),
		strdup("-v"),
		strdup("rejected-positional"),
		nullptr
	};
	constexpr int argc = std::size(argv) - 1;

	int calls = 0;
	const bool result = mh::parse_args(argc, argv, "v", [&](const mh::parsed_option& opt)
	{
		calls++;
		// regression: returning false for a non-option argument used to be
		// silently ignored and parse_args still reported success
		return !opt.is_non_option;
	});

	REQUIRE(result == false);
	REQUIRE(calls == 2); // '-v' accepted, 'rejected-positional' rejected and parsing aborted

	for (char* arg : argv)
		free(arg);
}
#endif
