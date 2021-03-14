#include "mh/concurrency/thread_pool.hpp"
#include "mh/coroutine/task.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <catch2/catch.hpp>

#include <atomic>
#include <cstring>
#include <iostream>
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

#if 0
TEST_CASE("task - exception rethrow bug minimal example")
{
	struct test_exception
	{

	};

	struct throwing_coroutine
	{
		bool await_ready() const { return false; }
		void await_resume() const
		{
			throw test_exception{};
		}
		void await_suspend(std::coroutine_handle<> parent)
		{
			parent.resume();
		}
	} static s_ThrowingCoroutine;

	struct rethrowing_promise
	{
		rethrowing_promise()
		{
			try
			{
				throw test_exception{};
			}
			catch (...)
			{
				m_Exception = std::current_exception();
			}
		}

		bool await_ready() const { return true; }
		void await_resume() const
		{
			std::rethrow_exception(m_Exception);
		}
		void await_suspend(std::coroutine_handle<>)
		{
			assert(!"Should never get here");
		}

		std::exception_ptr m_Exception;
	};

	struct rethrowing_coroutine
	{
#if 0
		rethrowing_coroutine()
		{
			try
			{
				throw test_exception{};
			}
			catch (...)
			{
				m_Exception = std::current_exception();
			}
		}

		bool await_ready() const { return true; }
		void await_resume() const
		{
			std::rethrow_exception(m_Exception);
		}
		void await_suspend(std::coroutine_handle<>)
		{
			assert(!"Should never get here");
		}

		std::exception_ptr m_Exception;
#else
		using promise_type = rethrowing_promise;
		promise_type operator co_await() const { return rethrowing_promise{}; }
#endif
	};

	mh::thread_pool tp(2);
	int test_value = 0;
	auto task = [](mh::thread_pool& tp, int& test_value) -> mh::task<>
	{
		//std::this_thread::sleep_for(1s);
		//co_await tp.co_add_task();

		throwing_coroutine example;
		try
		{
			co_await example;
		}
		catch (const test_exception& e)
		{
			test_value = 1234;
		}
	}(tp, test_value);

	task.wait();

	REQUIRE(test_value == 1234);
}
#endif

#if 1
TEST_CASE("task - exceptions from other threads")
{
	[[maybe_unused]] auto index = GENERATE(range(0, 50));

	//constexpr int EXPECTED_INT = 2342;
	constexpr int THROWN_INT = 9876;

	mh::thread_pool tp(2);
	auto producerTask = [](mh::thread_pool& tp) -> mh::task<>
	{
		co_await tp.co_add_task();
		std::this_thread::sleep_for(50ms);
		throw dummy_exception{ THROWN_INT };
	}(tp);

	CHECK(!producerTask.m_Handle.done());

	//producerTask.wait();

	int eValue = 0;
	auto consumerTask = [](mh::task<> producerTask, int& eValue, mh::thread_pool& tp) -> mh::task<>
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

	[[maybe_unused]] const auto& address = consumerTask.m_Handle.address();
	[[maybe_unused]] const auto& promise = consumerTask.m_Handle.promise();

	// Just some random waits that should be valid
	producerTask.wait();
	CHECK(producerTask.m_Handle.done());
	consumerTask.wait();
	CHECK(consumerTask.m_Handle.done());
	REQUIRE(eValue == THROWN_INT);

	consumerTask.wait();
	CHECK(consumerTask.m_Handle.done());
	producerTask.wait();
	CHECK(producerTask.m_Handle.done());
	REQUIRE(eValue == THROWN_INT);
}
#endif

TEST_CASE("task - exceptions in discarded tasks")
{
	mh::thread_pool tp(2);

	int value = 0;
	[](mh::thread_pool& tp, int& val) -> mh::task<>
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

#endif
