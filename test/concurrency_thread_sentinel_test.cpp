#include "mh/concurrency/thread_sentinel.hpp"

#include <catch2/catch_all.hpp>

#include <exception>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>

// NOTE: all check() calls pass the location explicitly: on toolchains without
// std::source_location (e.g. clang-14/libc++-14), MH_SOURCE_LOCATION_AUTO
// degrades to a parameter with no default argument.

TEST_CASE("thread_sentinel - check passes on the constructing thread", "[concurrency][thread_sentinel]")
{
	const mh::thread_sentinel sentinel;
	CHECK_NOTHROW(sentinel.check(MH_SOURCE_LOCATION_CURRENT()));
	CHECK_NOTHROW(sentinel.check(MH_SOURCE_LOCATION_CURRENT())); // still fine when checked repeatedly
}

TEST_CASE("thread_sentinel - check from another thread throws with details", "[concurrency][thread_sentinel]")
{
	STATIC_CHECK(std::is_base_of_v<std::runtime_error, mh::thread_sentinel_exception>);

	const mh::thread_sentinel sentinel;

	std::exception_ptr thrown;
	mh::source_location checkLocation;
	std::thread worker([&]
		{
			checkLocation = MH_SOURCE_LOCATION_CURRENT();
			try
			{
				sentinel.check(checkLocation);
			}
			catch (...)
			{
				thrown = std::current_exception();
			}
		});
	worker.join();

	REQUIRE(thrown);

	try
	{
		std::rethrow_exception(thrown);
	}
	catch (const mh::thread_sentinel_exception& e)
	{
		const std::string_view what = e.what();
		CHECK(what.find("mh::thread_sentinel expected thread id") != std::string_view::npos);
		CHECK(what.find("but was triggered from thread") != std::string_view::npos);
		// the location is printed as file(line):function
		CHECK(what.find("concurrency_thread_sentinel_test") != std::string_view::npos);

		// the exception must carry the location it was triggered from
		CHECK(e.location().line() == checkLocation.line());
		CHECK(std::string_view(e.location().file_name()) == checkLocation.file_name());
	}
}

TEST_CASE("thread_sentinel - reset_id moves ownership to the calling thread", "[concurrency][thread_sentinel]")
{
	mh::thread_sentinel sentinel;
	CHECK_NOTHROW(sentinel.check(MH_SOURCE_LOCATION_CURRENT()));

	std::exception_ptr beforeReset, afterReset;
	std::thread worker([&]
		{
			try
			{
				sentinel.check(MH_SOURCE_LOCATION_CURRENT());
			}
			catch (...)
			{
				beforeReset = std::current_exception();
			}

			sentinel.reset_id(); // sentinel now expects the worker thread

			try
			{
				sentinel.check(MH_SOURCE_LOCATION_CURRENT());
			}
			catch (...)
			{
				afterReset = std::current_exception();
			}
		});
	worker.join();

	CHECK(beforeReset != nullptr); // wrong thread before the reset
	CHECK(afterReset == nullptr);  // owning thread after the reset

	// ...which makes the original thread the wrong one
	CHECK_THROWS_AS(sentinel.check(MH_SOURCE_LOCATION_CURRENT()), mh::thread_sentinel_exception);
}

TEST_CASE("thread_sentinel - reset_id with an explicit id", "[concurrency][thread_sentinel]")
{
	mh::thread_sentinel sentinel;

	// no thread has a default-constructed id, so every check must throw
	sentinel.reset_id(std::thread::id{});
	CHECK_THROWS_AS(sentinel.check(MH_SOURCE_LOCATION_CURRENT()), mh::thread_sentinel_exception);

	sentinel.reset_id(std::this_thread::get_id());
	CHECK_NOTHROW(sentinel.check(MH_SOURCE_LOCATION_CURRENT()));
}
