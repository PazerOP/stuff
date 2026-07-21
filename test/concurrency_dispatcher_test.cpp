#include "mh/concurrency/dispatcher.hpp"
#include "mh/coroutine/task.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace std::chrono_literals;

namespace
{
	// The wait_tasks()/wait_tasks_while() family was removed from mh::dispatcher:
	// the template overload never compiled (its function-pointer type was
	// ill-formed) and the non-template overloads were declared but never defined,
	// so every possible caller hit a compile or link error. The untimed
	// wait_tasks() has since been reintroduced - implemented and tested this
	// time (see the close()/shutdown tests below); wait_tasks_while() stays gone.
	template<typename T>
	concept has_wait_tasks_while = requires(const T& d)
	{
		d.wait_tasks_while(static_cast<bool (*)(void*)>(nullptr), static_cast<void*>(nullptr));
	};
}

TEST_CASE("dispatcher - wait_tasks_for/until report task availability")
{
	REQUIRE_FALSE(has_wait_tasks_while<mh::dispatcher>);

	mh::dispatcher d(false);

	// No tasks: the wait times out
	REQUIRE_FALSE(d.wait_tasks_for(10ms));
	REQUIRE_FALSE(d.wait_tasks_until(mh::dispatcher::clock_t::now() + 10ms));

	// With a queued task the wait returns (well before the timeout)
	std::atomic<bool> ran = false;
	mh::task<> t = [](mh::dispatcher& disp, std::atomic<bool>& ranFlag) -> mh::task<>
	{
		co_await disp.co_dispatch();
		ranFlag = true;
	}(d, ran);

	REQUIRE(d.wait_tasks_for(10s));
	REQUIRE(d.run() == 1);
	REQUIRE(ran);
	t.wait();
}

TEST_CASE("dispatcher - a newly added delay task wakes a parked waiter")
{
	// regression: add_delay_task() never notified the condition variable and the
	// waiter's wakeup deadline stayed pinned to its old value, so a thread parked
	// in wait_tasks_until() slept through newly added earlier deadlines and delay
	// tasks fired up to a full wait window late.
	mh::dispatcher d(false);

	std::atomic<bool> waiterStarted = false;
	std::chrono::milliseconds waited{};
	std::thread waiter([&d, &waiterStarted, &waited]
	{
		waiterStarted = true;
		const auto start = std::chrono::steady_clock::now();
		(void)d.wait_tasks_until(mh::dispatcher::clock_t::now() + 3s);
		waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
	});

	while (!waiterStarted)
		std::this_thread::yield();

	// Give the waiter time to actually park itself inside wait_tasks_until().
	// Not needed for correctness (if it has not parked yet, it just computes its
	// deadline from the already-present delay task); this only makes the measured
	// scenario - "delay added while a waiter is parked" - reliable.
	std::this_thread::sleep_for(100ms);

	std::atomic<bool> delayDone = false;
	mh::task<> t = [](mh::dispatcher& disp, std::atomic<bool>& done) -> mh::task<>
	{
		co_await disp.co_delay_for(50ms);
		done = true;
	}(d, delayDone);

	waiter.join();
	REQUIRE(waited < 2000ms); // used to sleep out the whole 3s window

	// The (now expired) delay task must be runnable
	REQUIRE(d.run() == 1);
	REQUIRE(delayDone);
	t.wait();
}

#ifndef _WIN32
namespace
{
	struct pipe_pair
	{
		pipe_pair() { REQUIRE(pipe(fd) == 0); }
		~pipe_pair()
		{
			close(fd[0]);
			close(fd[1]);
		}
		pipe_pair(const pipe_pair&) = delete;
		pipe_pair& operator=(const pipe_pair&) = delete;

		int fd[2] = { -1, -1 };
	};
}

TEST_CASE("dispatcher - resumes every coroutine whose fd is ready")
{
	// regression: when multiple fd-wait coroutines became ready in the same poll,
	// the dispatcher used to resume only the first one and silently dropped the
	// rest - they vanished from every queue and could never resume.
	pipe_pair pipe1, pipe2;

	mh::dispatcher d;
	bool done1 = false, done2 = false;

	auto waitRead = [](mh::dispatcher& disp, int fd, bool& done) -> mh::task<>
	{
		co_await disp.co_wait_fd_read(fd);
		done = true;
	};

	mh::task<> t1 = waitRead(d, pipe1.fd[0], done1);
	mh::task<> t2 = waitRead(d, pipe2.fd[0], done2);
	REQUIRE(d.task_count() == 2);

	// Make both fds readable before the dispatcher polls
	REQUIRE(write(pipe1.fd[1], "x", 1) == 1);
	REQUIRE(write(pipe2.fd[1], "x", 1) == 1);

	const size_t resumed = d.run();
	REQUIRE(resumed == 2);
	REQUIRE(done1);
	REQUIRE(done2);
	REQUIRE(d.task_count() == 0);

	t1.wait();
	t2.wait();
}
#endif

TEST_CASE("dispatcher - task_count and try_pop are safe with concurrent producers")
{
	// regression: try_pop_task() and task_count() used to read the task queues
	// without taking the mutex while producer threads pushed under it - a data
	// race. The assertions here check the functional outcome (nothing is lost or
	// double-run); the race itself is caught by running this test under
	// ThreadSanitizer, which is done out-of-band / in sanitizer CI runs.
	constexpr size_t PRODUCERS = 4;
	constexpr size_t TASKS_PER_PRODUCER = 250;
	constexpr size_t TOTAL = PRODUCERS * TASKS_PER_PRODUCER;

	mh::dispatcher d(false);
	std::atomic<size_t> resumed = 0;
	std::vector<mh::task<>> tasks(TOTAL);

	std::vector<std::thread> producers;
	for (size_t p = 0; p < PRODUCERS; p++)
	{
		producers.emplace_back([&d, &resumed, &tasks, p]
		{
			for (size_t i = 0; i < TASKS_PER_PRODUCER; i++)
			{
				tasks[p * TASKS_PER_PRODUCER + i] = [](mh::dispatcher& disp, std::atomic<size_t>& counter) -> mh::task<>
				{
					co_await disp.co_dispatch();
					counter.fetch_add(1);
				}(d, resumed);
			}
		});
	}

	// Drain the dispatcher on this thread while the producers are still pushing,
	// polling task_count() the whole time (the other half of the old race).
	const auto deadline = std::chrono::steady_clock::now() + 25s;
	while (resumed.load() < TOTAL && std::chrono::steady_clock::now() < deadline)
	{
		(void)d.task_count();
		if (d.run() == 0)
			(void)d.wait_tasks_for(1ms);
	}

	for (auto& producer : producers)
		producer.join();

	REQUIRE(resumed.load() == TOTAL);
	REQUIRE(d.task_count() == 0);

	size_t ready = 0;
	for (const auto& t : tasks)
	{
		if (t.is_ready())
			ready++;
	}
	REQUIRE(ready == TOTAL);
}

TEST_CASE("dispatcher - close wakes parked waiters")
{
	// Threads parked in the wait_tasks family (with or without a deadline)
	// must wake promptly when the dispatcher is closed, instead of sleeping
	// out their windows - this is what makes thread_pool destruction prompt.
	mh::dispatcher d(false);
	REQUIRE_FALSE(d.is_closed());

	std::atomic<bool> timedStarted = false, untimedStarted = false;
	std::atomic<bool> timedResult = true, untimedResult = true;

	std::thread timedWaiter([&d, &timedStarted, &timedResult]
	{
		timedStarted = true;
		timedResult = d.wait_tasks_for(10s);
	});
	std::thread untimedWaiter([&d, &untimedStarted, &untimedResult]
	{
		untimedStarted = true;
		untimedResult = d.wait_tasks();
	});

	while (!timedStarted || !untimedStarted)
		std::this_thread::yield();

	// Give both waiters time to actually park themselves (reliability of the
	// measured scenario, not correctness - see the parked-waiter test above).
	std::this_thread::sleep_for(100ms);

	const auto start = std::chrono::steady_clock::now();
	d.close();
	timedWaiter.join();
	untimedWaiter.join();
	const auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE(d.is_closed());
	REQUIRE_FALSE(timedResult);   // woken by close, not a task
	REQUIRE_FALSE(untimedResult);
	REQUIRE(elapsed < 5000ms);    // neither slept out its window (10s/forever)
}

TEST_CASE("dispatcher - close drains ready work and rejects new awaits")
{
	mh::dispatcher d(false);

	// One task ready to run, one parked on a far-future delay
	std::atomic<bool> readyRan = false;
	mh::task<> ready = [](mh::dispatcher& disp, std::atomic<bool>& ran) -> mh::task<>
	{
		co_await disp.co_dispatch();
		ran = true;
	}(d, readyRan);

	std::atomic<bool> delayedRan = false;
	mh::task<> delayed = [](mh::dispatcher& disp, std::atomic<bool>& ran) -> mh::task<>
	{
		co_await disp.co_delay_for(10s);
		ran = true;
	}(d, delayedRan);

	REQUIRE(d.task_count() == 2);
	d.close();

	// Both are now runnable: the queued task completes normally (work that
	// was accepted is not dropped), the flushed delay completes by throwing
	// (its deadline never arrived).
	REQUIRE(d.wait_tasks());
	REQUIRE(d.run() == 2);
	REQUIRE(readyRan);
	ready.wait();
	REQUIRE(ready.get_exception() == nullptr);
	REQUIRE_FALSE(delayedRan);
	delayed.wait();
	REQUIRE(delayed.get_exception() != nullptr);

	// co_await after close() must not park a coroutine nothing will ever
	// resume: it throws, and the exception surfaces through the task like
	// any other.
	mh::task<> late = [](mh::dispatcher& disp) -> mh::task<>
	{
		co_await disp.co_dispatch();
	}(d);
	late.wait();
	REQUIRE(late.get_exception() != nullptr);

	mh::task<> lateDelay = [](mh::dispatcher& disp) -> mh::task<>
	{
		co_await disp.co_delay_for(10s);
	}(d);
	lateDelay.wait();
	REQUIRE(lateDelay.get_exception() != nullptr);

	REQUIRE(d.task_count() == 0);
	REQUIRE_FALSE(d.wait_tasks()); // closed with nothing left: no more waiting
}

TEST_CASE("dispatcher - registration slot is cleared on destruction")
{
	REQUIRE(mh::dispatcher::try_get() == nullptr);

	{
		mh::dispatcher d1;
		d1.register_for_current_thread();
		REQUIRE(mh::dispatcher::try_get() == &d1);
		REQUIRE(&mh::dispatcher::get() == &d1);
	}

	// regression: the thread-local registration used to outlive the dispatcher,
	// so try_get()/get() returned a dangling pointer and registering any new
	// dispatcher on this thread threw "already registered" forever.
	REQUIRE(mh::dispatcher::try_get() == nullptr);

	mh::dispatcher d2;
	REQUIRE_NOTHROW(d2.register_for_current_thread());
	REQUIRE(mh::dispatcher::try_get() == &d2);
} // d2 unregisters itself here, leaving the thread clean for other tests

#endif
