#pragma once

#include <atomic>
#include <mutex>

#if __has_include(<coroutine>)
#include <coroutine>
namespace mh::detail::eager_task_hpp
{
	namespace stdcoro = std;
}
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
namespace mh::detail::eager_task_hpp
{
	namespace stdcoro = std::experimental;
}
#endif

namespace mh
{
	template<typename T> class eager_task;

	namespace detail::eager_task_hpp
	{
		static constexpr bool AWAIT_READY_RESUME = true;
		static constexpr bool AWAIT_READY_DEFER = false;

		// Cancel the suspension. On this thread, immediately resume the coroutine we were about to suspend.
		static constexpr bool AWAIT_SUSPEND_RESUME = false;

		// Put ourselves to sleep for now. Someone else will wake us back up later.
		static constexpr bool AWAIT_SUSPEND_DEFER = true;

		template<typename T> using remove_rvalue_reference_t = std::conditional_t<std::is_rvalue_reference_v<T>, std::remove_reference_t<T>, T>;

		template<typename T> constexpr auto coro_value_helper(int) -> decltype(std::declval<T>().await_resume()) {}
		template<typename T> constexpr auto coro_value_helper(void*) ->
			decltype(coro_value_helper<decltype(std::declval<T>().operator co_await())>(0)) {}
		template<typename T> using coro_value_t = remove_rvalue_reference_t<decltype(coro_value_helper<T>(0))>;

		template<typename T>
		struct promise_type_base
		{
			auto initial_suspend() const
			{
				return detail::eager_task_hpp::stdcoro::suspend_never{};
			}

			struct final_awaitable
			{
				bool await_ready() const
				{
					//__debugbreak();
					return false;
				}
				void await_resume()
				{
					__debugbreak();
				}

				template<typename TPromise>
				void await_suspend(
					stdcoro::coroutine_handle<TPromise> coroutine)
				{
					auto& promise = coroutine.promise();
					std::lock_guard lock(promise.m_ContinuationMutex);
					//return promise.m_Continuation;
					if (promise.m_Continuation)
						promise.m_Continuation.resume();
				}
			};

			auto final_suspend() const
			{
				return final_awaitable{};

				//__debugbreak();
				//assert(m_NeedsResume);
				//m_NeedsResume = false;
				//throw "Not implemented";
				//return detail::eager_task_hpp::stdcoro::suspend_always{};
			}

			//bool needs_resume() const { return m_NeedsResume; }

			bool try_set_continuation(stdcoro::coroutine_handle<> continuation)
			{
				std::lock_guard lock(m_ContinuationMutex);
				if (m_ContinuationChecked)
					return false;

				m_Continuation = continuation;
				return true;
			}

		private:
			std::mutex m_ContinuationMutex;
			bool m_ContinuationChecked = false;
			stdcoro::coroutine_handle<> m_Continuation;
		};

		template<typename T>
		struct promise_type : promise_type_base<T>
		{
			void return_value(T&& value)
			{
				assert(!m_Result.has_value());
				m_Result = std::move(value);
			}
			void return_value(const T& value)
			{
				assert(!m_Result.has_value());
				m_Result = value;
			}

			eager_task<T> get_return_object()
			{
				return stdcoro::coroutine_handle<promise_type>::from_promise(*this);
			}

			bool is_ready() const { return m_Result.has_value(); }
			const T& value() const { return m_Result.value(); }

		private:
			std::optional<T> m_Result;
		};

		template<>
		struct promise_type<void> : promise_type_base<void>
		{
			void return_void()
			{
				m_IsReady = true;
				//__debugbreak();
			}

			eager_task<void> get_return_object();

			bool is_ready() const { return m_IsReady; }
			void value() const {}

		private:
			bool m_IsReady = false;
		};
	}

	template<typename T = void>
	class eager_task
	{
	public:
		using promise_type = detail::eager_task_hpp::promise_type<T>;

		eager_task(detail::eager_task_hpp::stdcoro::coroutine_handle<promise_type> handle) :
			m_Handle(handle)
		{
		}

		T await_resume()
		{
			bool expected = false;
			bool desired = true;
			assert(m_Handle.done());
			//if (m_Handle.promise().m_IsResumed.compare_exchange_strong(expected, desired))
			//{
			//	assert(!m_Handle.done());
			//	m_Handle.resume();
			//}
			//else
			{
				//assert(!m_Handle.promise().needs_resume());
				while (!m_Handle.done())
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			assert(m_Handle.done());

			return m_Handle.promise().value();

			//__debugbreak();
		}

		// What should we do if await_ready() returns false? We need to define how we
		// "go to sleep" and ensure that someone else will wake us up again later.
#if 0
		detail::eager_task_hpp::stdcoro::coroutine_handle<> await_suspend(
			detail::eager_task_hpp::stdcoro::coroutine_handle<> handle)
		{
			// Already in progress, nothing to start
			return stdcoro::noop_coroutine();
			//return handle;

			// TODO is this correct?
			//return m_Handle;
			//__debugbreak(); return false;
		}
#else
		bool await_suspend(detail::eager_task_hpp::stdcoro::coroutine_handle<> handle)
		{
			// Don't need to resume, we're already in progress

			using namespace detail::eager_task_hpp;
			if (m_Handle.promise().try_set_continuation(handle))
				return AWAIT_SUSPEND_DEFER; // This means we are suspending <handle>, NOT <m_Handle>

			return AWAIT_SUSPEND_RESUME; // value is ready or something
		}
#endif
		bool await_ready()
		{
			using namespace detail::eager_task_hpp;
			if (!m_Handle.promise().is_ready())
				return AWAIT_READY_DEFER;
			else
				return AWAIT_READY_RESUME;
		}

	private:
		detail::eager_task_hpp::stdcoro::coroutine_handle<promise_type> m_Handle;
	};

	inline eager_task<void> detail::eager_task_hpp::promise_type<void>::get_return_object()
	{
		return stdcoro::coroutine_handle<promise_type>::from_promise(*this);
	}

	template<typename Task>
	inline eager_task<detail::eager_task_hpp::coro_value_t<Task>> make_eager_task(Task task)
	{
		co_return co_await task;
	}

	template<typename T>
	inline eager_task<T> make_eager_task(eager_task<T>&& task)
	{
		return std::move(task);
	}
}
