#include "mh/synchronization/semaphore.hpp"
#include "catch2/repo/single_include/catch2/catch.hpp"

#include <future>
#include <iostream>

TEST_CASE("semaphore_basics")
{
	std::future<void> test[4];
	mh::counting_semaphore semaphore(0);
	for (size_t i = 0; i < std::size(test); i++)
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
