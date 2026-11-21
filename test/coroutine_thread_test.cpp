#include "mh/concurrency/main_thread.hpp"
#include "mh/coroutine/task.hpp"
#include "mh/coroutine/thread.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <catch2/catch_all.hpp>

#include <thread>

TEST_CASE("co_create_thread - resumes on a new thread", "[coroutine][thread]")
{
	std::thread::id resumedThread{};

	mh::task<> t = [](std::thread::id& outID) -> mh::task<>
	{
		co_await mh::co_create_thread();
		outID = std::this_thread::get_id();
	}(resumedThread);
	t.wait();

	CHECK(resumedThread != std::thread::id{});
	CHECK(resumedThread != std::this_thread::get_id());
}

TEST_CASE("co_create_background_thread - moves off the main thread", "[coroutine][thread]")
{
	REQUIRE(mh::is_main_thread());

	std::thread::id resumedThread{};

	mh::task<> t = [](std::thread::id& outID) -> mh::task<>
	{
		co_await mh::co_create_background_thread();
		outID = std::this_thread::get_id();
	}(resumedThread);
	t.wait();

	CHECK(resumedThread != std::thread::id{});
	CHECK(resumedThread != std::this_thread::get_id());
}

TEST_CASE("co_create_background_thread - stays on a non-main thread", "[coroutine][thread]")
{
	std::thread::id workerThread{};
	std::thread::id resumedThread{};

	std::thread worker([&]
		{
			workerThread = std::this_thread::get_id();

			mh::task<> t = [](std::thread::id& outID) -> mh::task<>
			{
				co_await mh::co_create_background_thread();
				outID = std::this_thread::get_id();
			}(resumedThread);
			t.wait();
		});
	worker.join();

	CHECK(workerThread != std::thread::id{});
	// already off the main thread: execution must NOT move to a new thread
	CHECK(resumedThread == workerThread);
}

#endif
