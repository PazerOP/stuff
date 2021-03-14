#include "mh/concurrency/thread_pool.hpp"
#include "mh/coroutine/task.hpp"

#include <catch2/catch.hpp>

#include <atomic>
#include <thread>

#ifdef _WIN32
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

TEST_CASE("task - standard threaded std::nested_expection sanity check")
{
	auto dummy_exception_ptr = []() -> std::exception_ptr
	{
		try
		{
			throw dummy_exception(__LINE__);
		}
		catch (const dummy_exception& e)
		{
			return std::current_exception();
		}

		assert(false);
		return nullptr;
	}();

	try
	{
		//[&]() -> mh::task<>
		//{
			try
			{
				//std::rethrow_exception(dummy_exception_ptr);
				throw dummy_exception(__LINE__);
			}
			catch (const dummy_exception& e)
			{
				throw dummy_exception(__LINE__);
			}
		//}();
	}
	catch (...)
	{

	}
}

TEST_CASE("task - exceptions from other threads")
{
	constexpr int EXPECTED_INT = 2342;
	constexpr int THROWN_INT = 9876;

	mh::thread_pool tp(2);
	auto producerTask = [](mh::thread_pool& tp) -> mh::task<>
	{
		co_await tp.co_add_task();
		std::this_thread::sleep_for(2s);
		throw dummy_exception{ THROWN_INT };
	}(tp);

	int eValue = 0;
	auto consumerTask = [](mh::task<> producerTask, int& eValue, mh::thread_pool& tp) -> mh::task<>
	{
		try
		{
			//co_await tp.co_add_task();
			co_await producerTask;
		}
		catch (const dummy_exception& e)
		{
			eValue = e.m_Value;
			//throw dummy_exception(__LINE__);
		}

		__debugbreak();
	}(producerTask, eValue, tp);

	const auto& address = consumerTask.m_Handle.address();
	const auto& promise = consumerTask.m_Handle.promise();

	// Just some random waits that should be valid
	consumerTask.wait();
	producerTask.wait();
	producerTask.wait();
	consumerTask.wait();

	REQUIRE(eValue == THROWN_INT);
}

TEST_CASE("task - exceptions in discarded tasks")
{
	mh::thread_pool tp(2);

	int value = 0;
	[](mh::thread_pool& tp, int& val) -> mh::task<>
	{
		co_await tp.co_add_task();
		std::this_thread::sleep_for(2s);
		val = 50030;
		throw dummy_exception(__LINE__);
		val = 30234;

	}(tp, value);

	std::this_thread::sleep_for(3s);
	REQUIRE(value == 50030);
}
