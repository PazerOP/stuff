#include "mh/concurrency/thread_pool.hpp"
#include "mh/coroutine/task.hpp"
#include "mh/coroutine/thread.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include "last_include.hpp"
#endif

using namespace std::chrono_literals;

struct test_noncopyable_struct
{
	static constexpr char TEST_STR[] = "A string that does not fit into the buffer for SSO.";

	test_noncopyable_struct()
	{
		value = TEST_STR;
	}
	~test_noncopyable_struct()
	{
		assert(!strcmp(value.c_str(), TEST_STR));
	}

	std::string value;
};

#ifdef _WIN32
#define PRINT_EX_MSG() \
	char buf[256]; \
	sprintf_s(buf, "%s(%i | %p)\n", __FUNCSIG__, m_InstanceIndex, this); \
	OutputDebugStringA(buf);
#else
#define PRINT_EX_MSG() // Not implemented
#endif

struct dummy_exception : std::nested_exception
{
	inline static std::atomic<int32_t> s_InstanceCount;

	dummy_exception(int v) :
		m_Value(v)
	{
		PRINT_EX_MSG();
	}

	~dummy_exception()
	{
		PRINT_EX_MSG();
	}

#if 0
	dummy_exception(dummy_exception&& other) :
		std::nested_exception(std::move(other)),
		m_Value(other.m_Value)
	{
		PRINT_EX_MSG();
	}
	dummy_exception(const dummy_exception& other) :
		std::nested_exception(other),
		m_Value(other.m_Value)
	{
		PRINT_EX_MSG();
	}
#else
	dummy_exception(const dummy_exception& other) :
		std::nested_exception(other),
		m_Value(std::move(other.m_Value)),
		m_TestStruct(std::move(other.m_TestStruct))
	{
	}
	//dummy_exception(dummy_exception&&) = delete;
	//dummy_exception(const dummy_exception&) = delete;
#endif

	int m_Value;
	test_noncopyable_struct m_TestStruct;
	int m_InstanceIndex = s_InstanceCount++;
};

TEST_CASE("task - exceptions from other threads")
{
	auto index = GENERATE(range(0, 50));

	//constexpr int EXPECTED_INT = 2342;
	constexpr int THROWN_INT = 9876;

	mh::thread_pool tp(2);
	mh::task<> producerTask = [](mh::thread_pool& tp, int index) -> mh::task<>
	{
		co_await tp.co_add_task();
		std::this_thread::sleep_for(std::chrono::milliseconds(index));
		throw dummy_exception{ THROWN_INT };
	}(tp, index);

	//producerTask.wait();

	int eValue = 0;
	mh::task<> consumerTask = [](mh::task<> producerTask, int& eValue, mh::thread_pool& tp) -> mh::task<>
	{
		try
		{
			co_await tp.co_add_task();
			co_await producerTask;
		}
		catch (const dummy_exception& e)
		{
			eValue = e.m_Value;
			//REQUIRE(eValue == THROWN_INT);
			//throw dummy_exception(__LINE__);
		}
		catch (const std::exception& e)
		{
			FAIL("wat");
		}
		catch (...)
		{
			FAIL("Unknown exception, somehow");
		}

		//__debugbreak();
	}(producerTask, eValue, tp);

	// Just some random waits that should be valid
	if (index % 2)
	{
		producerTask.wait();
		consumerTask.wait();
	}
	else
	{
		consumerTask.wait();
		producerTask.wait();
	}
	REQUIRE(eValue == THROWN_INT);
}

TEST_CASE("task - exceptions in discarded tasks")
{
	mh::thread_pool tp(2);

	std::atomic<int> value = 0;
	// Intentionally discarding the task - cast to void to suppress nodiscard warning
	(void)[](mh::thread_pool& tp, std::atomic<int>& val) -> mh::task<>
	{
		co_await tp.co_add_task();
		co_await tp.co_delay_for(2s);
		//std::this_thread::sleep_for(2s);
		val = 50030;
		throw dummy_exception(__LINE__);
		val = 30234;

	}(tp, value);

	std::this_thread::sleep_for(3s);
	REQUIRE(value == 50030);
}

#if !defined(__clang_major__) || (__clang_major__ >= 10)
TEST_CASE("task - contained object lifetime")
{
	struct DeletionMarker final
	{
		DeletionMarker(bool& isDeleted) :
			m_IsDeleted(&isDeleted)
		{
			assert(m_IsDeleted);
			assert(!*m_IsDeleted);
		}
		~DeletionMarker()
		{
			if (m_IsDeleted)
				*m_IsDeleted = true;
		}

		DeletionMarker(const DeletionMarker&) = delete;
		DeletionMarker(DeletionMarker&& other) :
			m_IsDeleted(std::exchange(other.m_IsDeleted, nullptr))
		{
		}
		DeletionMarker& operator=(const DeletionMarker&) = delete;
		DeletionMarker& operator=(DeletionMarker&& other) = delete;

		bool* m_IsDeleted;
	};

	bool isObjectDeleted = false;

	auto coroutine = [&isObjectDeleted]() -> mh::task<DeletionMarker>
	{
		DeletionMarker marker(isObjectDeleted);

		co_return marker;
	};

	REQUIRE(!isObjectDeleted);

	{
		mh::task<DeletionMarker> result = coroutine();
		REQUIRE(!isObjectDeleted);
		result.wait();
		REQUIRE(!isObjectDeleted);
		std::this_thread::sleep_for(1s);
		REQUIRE(!isObjectDeleted);
	}

	std::this_thread::sleep_for(1s);
	REQUIRE(isObjectDeleted);
}
#endif

namespace
{
	mh::task<int> make_ready_coroutine_task(int value)
	{
		co_return value;
	}
}

TEST_CASE("task - self-assignment keeps the task alive")
{
	// regression: self copy-assignment used to call release() first, which
	// dropped the last reference and destroyed the live state before copying
	// the (now nulled) handle from itself.
	mh::task<int> t = make_ready_coroutine_task(7);
	t.wait();

	mh::task<int>* alias = &t; // assign through an alias to defeat self-assign warnings
	t = *alias;
	REQUIRE(t.valid());
	REQUIRE(t.is_ready());
	REQUIRE(t.get() == 7);

	// Move self-assignment used to hit the same destructive path in release builds
	// (only an assert() guarded it).
	t = std::move(*alias);
	REQUIRE(t.valid());
	REQUIRE(t.get() == 7);
}

TEST_CASE("task - moved-from tasks are empty")
{
	// regression: a user-provided (empty) destructor suppressed task's implicit
	// move operations, so every "move" was actually a refcount copy and the
	// moved-from task stayed attached to the shared state.
	mh::task<int> t1 = make_ready_coroutine_task(7);
	t1.wait();

	mh::task<int> t2 = std::move(t1);
	REQUIRE(t1.empty()); // intentional use-after-move: this IS the regression check
	REQUIRE(!t1.valid());
	REQUIRE(t1.state() == mh::task_state::empty);
	REQUIRE(t2.get() == 7);

	mh::task<int> t3;
	t3 = std::move(t2);
	REQUIRE(t2.empty());
	REQUIRE(t3.get() == 7);

	// Copies still copy - both handles stay attached
	mh::task<int> t4 = t3;
	REQUIRE(t3.valid());
	REQUIRE(t4.valid());
	REQUIRE(t4.get() == 7);
}

TEST_CASE("task - make_ready_task returns a ready task")
{
	// NOTE: make_ready_task allocates a non-coroutine promise with new; those
	// used to leak unconditionally (the delete branch was unreachable). Freeing
	// cannot be asserted from within the process; it is verified out-of-band by
	// running this test under AddressSanitizer/LeakSanitizer. This test pins
	// down the functional behavior of the same code path.
	auto t = mh::make_ready_task<int>(42);
	REQUIRE(t.valid());
	REQUIRE(t.is_ready());
	REQUIRE(t.state() == mh::task_state::value);
	REQUIRE(t.get() == 42);

	auto ts = mh::make_ready_task<std::string>("a value that is long enough to defeat SSO");
	REQUIRE(ts.get() == "a value that is long enough to defeat SSO");

	// Copies share the same non-coroutine state; destroying all of them must be
	// safe (and, under ASan, must free the state exactly once).
	auto tCopy = t;
	REQUIRE(tCopy.get() == 42);
}

TEST_CASE("task - cross-thread completion and abandonment stress")
{
	// regression: the coroutine frame used to be destroyed by the thread that
	// dropped the last task reference while the completing thread was still
	// inside resume() (use-after-free), and is_ready() used to read the result
	// state without synchronization. This test drives exactly that scenario;
	// the races themselves are caught by running it under Thread/AddressSanitizer,
	// which is done out-of-band / in sanitizer CI runs.
	constexpr int ITERATIONS = 64;

	// Waited-on tasks: this thread polls is_ready() (a cross-thread state read),
	// reads the value, then immediately destroys its task reference - racing the
	// worker thread's final-suspend bookkeeping.
	for (int i = 0; i < ITERATIONS; i++)
	{
		mh::task<int> t = [](int value) -> mh::task<int>
		{
			co_await mh::co_create_thread();
			co_return value;
		}(i);

		while (!t.is_ready())
			std::this_thread::yield();

		REQUIRE(t.get() == i);
	} // t destroyed here, possibly while the worker is still finishing resume()

	// Abandoned tasks: dropping the last reference before the coroutine finishes
	// hands frame ownership to the coroutine itself. Every abandoned coroutine
	// must still run to completion (frame destruction is verified by ASan).
	std::atomic<int> bodiesCompleted = 0;
	for (int i = 0; i < ITERATIONS; i++)
	{
		(void)[](std::atomic<int>& counter) -> mh::task<>
		{
			co_await mh::co_create_thread();
			counter.fetch_add(1);
		}(bodiesCompleted);
	} // the task handle is discarded immediately, before the coroutine completes

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (bodiesCompleted.load() < ITERATIONS && std::chrono::steady_clock::now() < deadline)
		std::this_thread::yield();

	REQUIRE(bodiesCompleted.load() == ITERATIONS);
}

#endif
