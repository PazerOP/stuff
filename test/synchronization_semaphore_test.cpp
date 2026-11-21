#include "mh/synchronization/semaphore.hpp"
#include <catch2/catch_all.hpp>

#include <chrono>
#include <future>
#include <iterator>
#include <thread>
#include "last_include.hpp"

TEST_CASE("semaphore_basics")
{
	std::future<void> test[4];
	mh::counting_semaphore<> semaphore(0);
	for (std::size_t i = 0; i < std::size(test); i++)
	{
		test[i] = std::async(std::launch::async, [&]
		{
			semaphore.acquire();
		});
	}

	for (int i = 0; i < 4; i++)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);
		semaphore.release();
	}
}

TEST_CASE("semaphore_try_acquire_for")
{
	using namespace std::chrono_literals;

	mh::counting_semaphore<> semaphore(0);
	CHECK_FALSE(semaphore.try_acquire_for(10ms));

	semaphore.release();
	CHECK(semaphore.try_acquire_for(10ms));
}

TEST_CASE("semaphore_try_acquire_until")
{
	using namespace std::chrono_literals;

	mh::counting_semaphore<> semaphore(0);
	CHECK_FALSE(semaphore.try_acquire_until(std::chrono::steady_clock::now() + 10ms));

	semaphore.release();
	CHECK(semaphore.try_acquire_until(std::chrono::steady_clock::now() + 10ms));
}
