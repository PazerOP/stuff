#include "mh/text/stringops.hpp"
#include <catch2/catch_all.hpp>
#include "last_include.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

TEST_CASE("trim - empty string", "[text][stringops]")
{
	std::string str;

	SECTION("trim_start")
	{
		str = mh::trim_start(std::move(str));
		REQUIRE(str.empty());
	}
	SECTION("trim_end")
	{
		str = mh::trim_end(std::move(str));
		REQUIRE(str.empty());
	}
	SECTION("trim")
	{
		str = mh::trim(std::move(str));
		REQUIRE(str.empty());
	}
}

TEST_CASE("trim - non-empty string", "[text][stringops]")
{
	std::string str = " hello.........\r.\n \t";

	SECTION("trim_start")
	{
		str = mh::trim_start(std::move(str));
		REQUIRE(str == "hello.........\r.\n \t");
	}
	SECTION("trim_end")
	{
		str = mh::trim_end(std::move(str));
		REQUIRE(str == " hello.........\r.");
	}
	SECTION("trim")
	{
		str = mh::trim(std::move(str));
		REQUIRE(str == "hello.........\r.");
	}
}

#ifdef MH_COROUTINES_SUPPORTED

namespace
{
	// split_string is a lazy coroutine generator; a regression that fails to
	// advance past a delimiter would yield pieces forever. Bound the number of
	// accepted pieces so such a regression fails fast instead of hanging.
	constexpr size_t MAX_SPLIT_PARTS = 100;

	template<typename TGenerator>
	static std::vector<std::string> collect_split_parts(TGenerator&& generator)
	{
		std::vector<std::string> parts;

		for (const std::string_view part : generator)
		{
			if (parts.size() >= MAX_SPLIT_PARTS)
				FAIL("split_string yielded more than " << MAX_SPLIT_PARTS << " pieces - the generator is not terminating");

			parts.emplace_back(part);
		}

		return parts;
	}
}

TEST_CASE("split_string - basic splitting", "[text][stringops][split_string]")
{
	using namespace std::string_view_literals;
	using parts_t = std::vector<std::string>;

	CHECK(collect_split_parts(mh::split_string("a,b"sv, ","sv)) == parts_t{ "a", "b" });
	CHECK(collect_split_parts(mh::split_string("hello world foo"sv, " "sv)) == parts_t{ "hello", "world", "foo" });
	CHECK(collect_split_parts(mh::split_string("no delimiters here!"sv, ","sv)) == parts_t{ "no delimiters here!" });
	CHECK(collect_split_parts(mh::split_string(""sv, ","sv)) == parts_t{ "" });
}

TEST_CASE("split_string - leading/trailing/adjacent delimiters", "[text][stringops][split_string]")
{
	using namespace std::string_view_literals;
	using parts_t = std::vector<std::string>;

	CHECK(collect_split_parts(mh::split_string("a,"sv, ","sv)) == parts_t{ "a", "" });
	CHECK(collect_split_parts(mh::split_string(",a"sv, ","sv)) == parts_t{ "", "a" });
	CHECK(collect_split_parts(mh::split_string("a,,b"sv, ","sv)) == parts_t{ "a", "", "b" });
	CHECK(collect_split_parts(mh::split_string("a;b,c"sv, ",;"sv)) == parts_t{ "a", "b", "c" });
}

TEST_CASE("split_string - convenience overload copies its inputs", "[text][stringops][split_string]")
{
	using parts_t = std::vector<std::string>;

	// The overload taking arbitrary string-likes creates temporary string_views.
	// The lazy generator must copy them into the coroutine frame; holding
	// references would leave the coroutine reading destroyed temporaries by the
	// time it is iterated.
	CHECK(collect_split_parts(mh::split_string("hello world", " ")) == parts_t{ "hello", "world" });
	CHECK(collect_split_parts(mh::split_string("x-y-z", "-")) == parts_t{ "x", "y", "z" });
}

#endif // MH_COROUTINES_SUPPORTED
