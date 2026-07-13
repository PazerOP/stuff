#include "mh/concurrency/main_thread.hpp"

#include <catch2/catch_all.hpp>

#include <thread>

TEST_CASE("main_thread - is_main_thread on the main thread", "[concurrency][main_thread]")
{
	// Catch2 runs test cases on the thread that entered main()
	CHECK(mh::main_thread_id == std::this_thread::get_id());
	CHECK(mh::is_main_thread());
}

TEST_CASE("main_thread - is_main_thread from a worker thread", "[concurrency][main_thread]")
{
	bool workerIsMain = true;
	std::thread::id mainIDSeenByWorker{};
	std::thread worker([&]
		{
			workerIsMain = mh::is_main_thread();
			mainIDSeenByWorker = mh::main_thread_id;
		});
	worker.join();

	CHECK(!workerIsMain);
	// main_thread_id is program-wide: every thread sees the same value
	CHECK(mainIDSeenByWorker == std::this_thread::get_id());
}
