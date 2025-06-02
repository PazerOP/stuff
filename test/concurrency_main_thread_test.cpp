#include "mh/concurrency/main_thread.hpp"
#include <catch2/catch_all.hpp>

#include <thread>
#include <atomic>
#include <future>
#include "last_include.hpp"

TEST_CASE("main_thread_id initialization", "[concurrency][main_thread]")
{
	SECTION("main_thread_id should be valid")
	{
		REQUIRE(mh::main_thread_id != std::thread::id{});
	}

	SECTION("main_thread_id should equal current thread id")
	{
		REQUIRE(mh::main_thread_id == std::this_thread::get_id());
	}
}

TEST_CASE("is_main_thread function", "[concurrency][main_thread]")
{
	SECTION("should return true on main thread")
	{
		REQUIRE(mh::is_main_thread() == true);
	}

	SECTION("should return false on different thread")
	{
		std::atomic<bool> is_main_from_thread{true}; // Start with true to detect if it changes
		
		std::thread test_thread([&is_main_from_thread]() {
			is_main_from_thread = mh::is_main_thread();
		});
		
		test_thread.join();
		
		REQUIRE(is_main_from_thread == false);
	}

	SECTION("should be consistent across multiple calls on main thread")
	{
		REQUIRE(mh::is_main_thread() == true);
		REQUIRE(mh::is_main_thread() == true);
		REQUIRE(mh::is_main_thread() == true);
	}

	SECTION("should be consistent across multiple calls on different thread")
	{
		std::atomic<int> false_count{0};
		
		std::thread test_thread([&false_count]() {
			for (int i = 0; i < 5; ++i) {
				if (!mh::is_main_thread()) {
					false_count++;
				}
			}
		});
		
		test_thread.join();
		
		REQUIRE(false_count == 5);
	}

	SECTION("different threads should have different ids")
	{
		std::thread::id thread1_id;
		std::thread::id thread2_id;
		
		std::thread thread1([&thread1_id]() {
			thread1_id = std::this_thread::get_id();
		});
		
		std::thread thread2([&thread2_id]() {
			thread2_id = std::this_thread::get_id();
		});
		
		thread1.join();
		thread2.join();
		
		REQUIRE(thread1_id != thread2_id);
		REQUIRE(thread1_id != mh::main_thread_id);
		REQUIRE(thread2_id != mh::main_thread_id);
		REQUIRE(thread1_id != std::thread::id{});
		REQUIRE(thread2_id != std::thread::id{});
	}

	SECTION("async task should not be on main thread")
	{
		auto future = std::async(std::launch::async, []() {
			return mh::is_main_thread();
		});
		
		bool is_main_from_async = future.get();
		REQUIRE(is_main_from_async == false);
	}
}