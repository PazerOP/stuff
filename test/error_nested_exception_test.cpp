#include "mh/error/nested_exception.hpp"
#include <catch2/catch_all.hpp>

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
	using visit_log = std::vector<std::pair<std::string, size_t>>;

	// throws runtime_error("outer") <- runtime_error("middle") <- runtime_error("innermost")
	[[noreturn]] void throwNestedChain()
	{
		try
		{
			try
			{
				throw std::runtime_error("innermost");
			}
			catch (...)
			{
				std::throw_with_nested(std::runtime_error("middle"));
			}
		}
		catch (...)
		{
			std::throw_with_nested(std::runtime_error("outer"));
		}
	}
}

TEST_CASE("for_each_nested_exception - visits every nested std::exception", "[error][nested_exception]")
{
	visit_log log;

	try
	{
		throwNestedChain();
	}
	catch (const std::runtime_error& e)
	{
		// default traversal order (top-down)
		mh::for_each_nested_exception(e, [&](const std::exception& nested, size_t depth)
			{ log.emplace_back(nested.what(), depth); });
	}

	// The exception passed in is not itself visited; its nested chain is.
	REQUIRE((log == visit_log{ { "middle", 0 }, { "innermost", 1 } }));
}

TEST_CASE("for_each_nested_exception - top-down and bottom-up report the same depths", "[error][nested_exception]")
{
	visit_log topDown, bottomUp;

	try
	{
		throwNestedChain();
	}
	catch (const std::runtime_error& e)
	{
		mh::for_each_nested_exception<false>(e, [&](const std::exception& nested, size_t depth)
			{ topDown.emplace_back(nested.what(), depth); });
		mh::for_each_nested_exception<true>(e, [&](const std::exception& nested, size_t depth)
			{ bottomUp.emplace_back(nested.what(), depth); });
	}

	REQUIRE((topDown == visit_log{ { "middle", 0 }, { "innermost", 1 } }));
	// bottom-up visits in reverse order, but each exception keeps its depth
	REQUIRE((bottomUp == visit_log{ { "innermost", 1 }, { "middle", 0 } }));
}

TEST_CASE("for_each_nested_exception - stops at a nested exception that is not a std::exception", "[error][nested_exception]")
{
	SECTION("directly nested unknown exception")
	{
		size_t visits = 0;

		try
		{
			try
			{
				throw 42; // not a std::exception; cannot be inspected for further nesting
			}
			catch (...)
			{
				std::throw_with_nested(std::runtime_error("wrapper"));
			}
		}
		catch (const std::runtime_error& e)
		{
			// must return (bounded), not recurse forever on the unknown exception
			mh::for_each_nested_exception(e, [&](const std::exception&, size_t) { ++visits; });
		}

		REQUIRE(visits == 0);
	}
	SECTION("unknown exception at the bottom of a std::exception chain")
	{
		visit_log log;

		try
		{
			try
			{
				try
				{
					throw 42;
				}
				catch (...)
				{
					std::throw_with_nested(std::runtime_error("middle"));
				}
			}
			catch (...)
			{
				std::throw_with_nested(std::runtime_error("outer"));
			}
		}
		catch (const std::runtime_error& e)
		{
			mh::for_each_nested_exception(e, [&](const std::exception& nested, size_t depth)
				{ log.emplace_back(nested.what(), depth); });
		}

		// the std::exception part of the chain is visited; the unknown exception ends it
		REQUIRE((log == visit_log{ { "middle", 0 } }));
	}
}

TEST_CASE("for_each_nested_exception - exception without any nesting", "[error][nested_exception]")
{
	size_t visits = 0;
	const std::runtime_error plain("no nesting here");
	mh::for_each_nested_exception(plain, [&](const std::exception&, size_t) { ++visits; });
	REQUIRE(visits == 0);
}
