#include "mh/concurrency/locked_value.hpp"

#include <catch2/catch_all.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

TEST_CASE("locked_value - type aliases", "[concurrency][locked_value]")
{
	using lv = mh::locked_value<std::string>;
	STATIC_CHECK(std::is_same_v<lv::value_type, std::string>);
	STATIC_CHECK(std::is_same_v<lv::mutex_type, std::mutex>);
	STATIC_CHECK(std::is_same_v<lv::lock_type, std::unique_lock<std::mutex>>);

	using lv_recursive = mh::locked_value<int, std::recursive_mutex>;
	STATIC_CHECK(std::is_same_v<lv_recursive::mutex_type, std::recursive_mutex>);
	STATIC_CHECK(std::is_same_v<lv_recursive::lock_type, std::unique_lock<std::recursive_mutex>>);
}

TEST_CASE("locked_value - construction", "[concurrency][locked_value]")
{
	SECTION("default")
	{
		const mh::locked_value<std::string> value;
		CHECK(value.get().empty());
	}
	SECTION("copy from initial value")
	{
		const std::string initial = "hello";
		const mh::locked_value<std::string> value(initial);
		CHECK(value.get() == "hello");
		CHECK(initial == "hello"); // not moved from
	}
	SECTION("move from initial value")
	{
		auto initial = std::make_shared<int>(5);
		const mh::locked_value<std::shared_ptr<int>> value(std::move(initial));
		CHECK(initial == nullptr); // moved from
		REQUIRE(value.get() != nullptr);
		CHECK(*value.get() == 5);
	}
}

TEST_CASE("locked_value - get/set and assignment", "[concurrency][locked_value]")
{
	mh::locked_value<std::shared_ptr<int>> value;

	SECTION("set copies lvalues")
	{
		const auto ptr = std::make_shared<int>(1);
		value.set(ptr);
		CHECK(ptr != nullptr);
		CHECK(ptr.use_count() == 2);
		CHECK(value.get() == ptr);
	}
	SECTION("set moves rvalues")
	{
		auto ptr = std::make_shared<int>(2);
		value.set(std::move(ptr));
		CHECK(ptr == nullptr);
		CHECK(*value.get() == 2);
	}
	SECTION("operator= copies lvalues")
	{
		const auto ptr = std::make_shared<int>(3);
		value = ptr;
		CHECK(ptr != nullptr);
		CHECK(value.get() == ptr);
	}
	SECTION("operator= moves rvalues")
	{
		auto ptr = std::make_shared<int>(4);
		value = std::move(ptr);
		CHECK(ptr == nullptr);
		CHECK(*value.get() == 4);
	}
	SECTION("implicit conversion returns a copy")
	{
		value.set(std::make_shared<int>(5));
		const std::shared_ptr<int> copy = value;
		REQUIRE(copy != nullptr);
		CHECK(*copy == 5);
		CHECK(copy.use_count() == 2); // the stored value and the copy
	}
}

TEST_CASE("locked_value - lock and get_ref", "[concurrency][locked_value]")
{
	mh::locked_value<int> value(1);

	{
		const auto lock = value.lock();
		REQUIRE(lock.owns_lock());

		value.get_ref(lock) = 42; // mutable reference under the lock
		CHECK(std::as_const(value).get_ref(lock) == 42); // const overload
	}

	CHECK(value.get() == 42);
}

TEST_CASE("locked_value - concurrent increments do not lose updates", "[concurrency][locked_value]")
{
	constexpr int THREADS = 4;
	constexpr int INCREMENTS = 10000;

	mh::locked_value<int> counter(0);

	std::vector<std::thread> threads;
	for (int t = 0; t < THREADS; t++)
	{
		threads.emplace_back([&counter]
			{
				for (int i = 0; i < INCREMENTS; i++)
				{
					const auto lock = counter.lock();
					++counter.get_ref(lock);
				}
			});
	}

	for (auto& thread : threads)
		thread.join();

	CHECK(counter.get() == THREADS * INCREMENTS);
}
