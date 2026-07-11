#include "mh/concurrency/thread_pool.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST_CASE("thread_pool - add_task runs the callable and returns its result")
{
	mh::thread_pool tp(2);
	REQUIRE(tp.thread_count() == 2);

	std::atomic<int> sum = 0;
	std::vector<mh::task<int>> tasks;
	for (int i = 0; i < 32; i++)
	{
		tasks.push_back(tp.add_task([&sum](int v)
			{
				sum += v;
				return v * 2;
			}, i));
	}

	int total = 0;
	for (auto& t : tasks)
	{
		t.wait();
		total += t.get();
	}

	REQUIRE(sum == 496);   // 0 + 1 + ... + 31
	REQUIRE(total == 992);
}

TEST_CASE("thread_pool - short delays on an idle pool complete promptly")
{
	// regression: a newly added delay task never woke the parked pool thread, so
	// a 50ms co_delay_for() on an idle pool completed only when the pool thread's
	// current 1s wait window expired - up to a full second late, every time.
	mh::thread_pool tp(1);

	std::chrono::milliseconds totalLatency{};
	for (int i = 0; i < 3; i++)
	{
		// Let the pool thread park itself in its wait window. Not needed for
		// correctness - only makes the measured scenario ("delay scheduled while
		// the pool is idle") reliable.
		std::this_thread::sleep_for(150ms);

		const auto start = std::chrono::steady_clock::now();
		mh::task<> t = [](mh::thread_pool& pool) -> mh::task<>
		{
			co_await pool.co_delay_for(50ms);
		}(tp);
		t.wait();
		totalLatency += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	}

	// Three 50ms delays: ~150ms of real delay, so 2s is extremely generous slack
	// for slow CI machines. Before the fix each delay cost ~1s (3s+ total).
	REQUIRE(totalLatency < 2000ms);
}

TEST_CASE("thread_pool - repeated construction and shutdown with work in flight")
{
	// regression: the shutdown flag was a plain bool written by the destructor
	// and read concurrently by every pool thread (a data race). The assertions
	// here check the functional outcome (all work completes, shutdown does not
	// hang or crash); the race itself is caught by running this test under
	// ThreadSanitizer, which is done out-of-band / in sanitizer CI runs.
	constexpr int POOLS = 8;
	constexpr int TASKS = 16;

	std::atomic<int> completed = 0;
	for (int p = 0; p < POOLS; p++)
	{
		mh::thread_pool tp(2);

		std::vector<mh::task<>> tasks;
		for (int i = 0; i < TASKS; i++)
		{
			tasks.push_back([](mh::thread_pool& pool, std::atomic<int>& counter) -> mh::task<>
			{
				co_await pool.co_add_task();
				counter.fetch_add(1);
			}(tp, completed));
		}

		for (auto& t : tasks)
			t.wait();
	} // pool destroyed while its threads may still be in their wait/run cycle

	REQUIRE(completed == POOLS * TASKS);
}

#endif
