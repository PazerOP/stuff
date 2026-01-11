#include "mh/concurrency/thread_pool.hpp"
#include "mh/coroutine/task.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <catch2/catch_all.hpp>

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
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

	int value = 0;
	// Intentionally discarding the task - cast to void to suppress nodiscard warning
	(void)[](mh::thread_pool& tp, int& val) -> mh::task<>
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

#endif
