#include "mh/io/getopt.hpp"

#if __has_include(<getopt.h>)
#include "catch2/repo/single_include/catch2/catch.hpp"

#include <array>
#include <string.h>

TEST_CASE("getopt")
{
	static const option options[] =
	{
		{ "file", required_argument, nullptr, 'f' },
		{ "directory", required_argument, nullptr, 'C' },
		{ "strip-components", required_argument },
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

		REQUIRE_THAT(opt.get_arg_name(), Catch::Equals(expected.get_arg_name()));

		REQUIRE(opt.get_arg_name_long() == expected_arg_name_long.at(index));
		REQUIRE(expected.get_arg_name_long() == expected_arg_name_long.at(index)); // Sanity check
		REQUIRE(opt.get_arg_name_short() == expected_arg_name_short.at(index));

		index++;
		return true;
	});

	REQUIRE(result == true);
	REQUIRE(index == actual_arg_count);
}
#endif
