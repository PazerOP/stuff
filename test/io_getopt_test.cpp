#include "mh/io/getopt.hpp"

#if __has_include(<getopt.h>)
#include <catch2/catch_all.hpp>

#include <array>
#include <iterator>
#include <string>
#include <sstream>
#include <stdexcept>
#include <string.h>
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

TEST_CASE("getopt - unknown option reports the offending character", "[io][getopt]")
{
	char* const argv[] =
	{
		strdup("prog"),
		strdup("-q"), // not in the option list
		nullptr
	};
	constexpr int argc = std::size(argv) - 1;

	int unknownCount = 0;
	// leading ':' suppresses getopt's own stderr diagnostics
	const bool result = mh::parse_args(argc, argv, ":v", [&](const mh::parsed_option& opt)
	{
		unknownCount++;
		REQUIRE(opt.getopt_result == mh::parsed_option::UNKNOWN_OPT_RESULT);
		// for '?', the short name comes from optopt (the rejected character)
		REQUIRE(opt.get_arg_name_short() == 'q');
		REQUIRE_THAT(opt.get_arg_name(), Catch::Matchers::Equals("q"));
		REQUIRE(opt.get_arg_name_long().empty());
		return true;
	});

	REQUIRE(result == true);
	REQUIRE(unknownCount == 1);

	for (char* arg : argv)
		free(arg);
}

TEST_CASE("getopt - callback can reject an option", "[io][getopt]")
{
	char* const argv[] =
	{
		strdup("prog"),
		strdup("-v"),
		nullptr
	};
	constexpr int argc = std::size(argv) - 1;

	int calls = 0;
	const bool result = mh::parse_args(argc, argv, "v", [&](const mh::parsed_option&)
	{
		calls++;
		return false; // reject the option itself (not a non-option argument)
	});

	REQUIRE(result == false);
	REQUIRE(calls == 1);

	for (char* arg : argv)
		free(arg);
}

TEST_CASE("getopt - longopt array validation", "[io][getopt]")
{
	char* const argv[] = { strdup("prog"), nullptr };
	constexpr int argc = std::size(argv) - 1;

	const auto accept_all = [](const mh::parsed_option&) { return true; };

	// an empty longopt range cannot hold its own null terminator
	const option validOpt{ "x", no_argument, nullptr, 'x' };
	REQUIRE_THROWS_AS(
		mh::parse_args(argc, argv, "x", &validOpt, &validOpt, accept_all),
		std::logic_error);

	// a range whose last element is not zeroed is not null-terminated
	const option notTerminated[] = { { "x", no_argument, nullptr, 'x' } };
	REQUIRE_THROWS_AS(
		mh::parse_args(argc, argv, "x", notTerminated, accept_all),
		std::logic_error);

	for (char* arg : argv)
		free(arg);
}

TEST_CASE("getopt - option ostream insertion", "[io][getopt]")
{
	{
		std::ostringstream os;
		os << option{ "opt-name", optional_argument, nullptr, 'o' };
		const std::string text = os.str();
		CHECK(text.find("\"opt-name\"") != std::string::npos);
		CHECK(text.find("optional_argument") != std::string::npos);
		CHECK(text.find("nullptr") != std::string::npos);
	}

	{
		// out-of-range has_arg values print numerically
		std::ostringstream os;
		os << option{ "bad", 42, nullptr, 'b' };
		CHECK(os.str().find(", 42,") != std::string::npos);
	}

	{
		// a null name prints "nullptr"; a flag pointer prints its target value
		int flag = 7;
		std::ostringstream os;
		os << option{ nullptr, no_argument, &flag, 1 };
		const std::string text = os.str();
		CHECK(text.find("nullptr") != std::string::npos);
		CHECK(text.find("&7") != std::string::npos);
		CHECK(text.find("no_argument") != std::string::npos);
	}
}

TEST_CASE("getopt - option comparison tie-breakers", "[io][getopt]")
{
	// null vs non-null name
	constexpr option nullName{ nullptr, no_argument, nullptr, 0 };
	constexpr option named{ "x", no_argument, nullptr, 0 };
	CHECK(nullName != named);
	CHECK(named != nullName);
	CHECK(nullName == nullName);

	// names equal (both null): has_arg breaks the tie
	constexpr option needsArg{ nullptr, required_argument, nullptr, 0 };
	CHECK(nullName != needsArg);

	// then the flag pointer (same array => well-defined ordering)
	static int flags[2] = {};
	const option flagA{ nullptr, no_argument, &flags[0], 0 };
	const option flagB{ nullptr, no_argument, &flags[1], 0 };
	CHECK(flagA != flagB);
	CHECK(flagA == flagA);

	// and finally val
	constexpr option valA{ nullptr, no_argument, nullptr, 1 };
	constexpr option valB{ nullptr, no_argument, nullptr, 2 };
	CHECK(valA != valB);
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
