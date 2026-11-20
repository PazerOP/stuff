#pragma once

#include "coroutine_include.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace mh
{
	template<typename T> class eager_task;

	namespace detail::eager_task_hpp
	{
		// await_ready() results: either resume here, or defer to the task itself.
		inline constexpr bool AWAIT_READY_RESUME = true;
		inline constexpr bool AWAIT_READY_DEFER = false;

		// await_suspend() results: suspend the awaiting coroutine, or keep running it.
		inline constexpr bool AWAIT_SUSPEND_DEFER = true;
		inline constexpr bool AWAIT_SUSPEND_RESUME = false;

		template<typename T> using remove_rvalue_reference_t = std::conditional_t<std::is_rvalue_reference_v<T>, std::remove_reference_t<T>, T>;

		template<typename T> constexpr auto coro_value_helper(int) -> decltype(std::declval<T>().await_resume()) {}
		template<typename T> constexpr auto coro_value_helper(void*) ->
			decltype(coro_value_helper<decltype(std::declval<T>().operator co_await())>(0)) {}
		template<typename T> using coro_value_t = remove_rvalue_reference_t<decltype(coro_value_helper<T>(0))>;

		template<typename T>
		struct promise_type_base
		{
			// Eager: the coroutine body runs before the caller receives the task.
			auto initial_suspend() const noexcept { return coro::suspend_never{}; }

			struct final_awaitable
			{
				bool await_ready() const noexcept { return false; }

				template<typename TPromise>
				void await_suspend(coro::coroutine_handle<TPromise> coroutine) noexcept
				{
					auto& promise = coroutine.promise();

					coro::coroutine_handle<> continuation;
					{
						std::lock_guard lock(promise.m_ContinuationMutex);
						promise.m_Finished = true;
						continuation = std::exchange(promise.m_Continuation, nullptr);
					}

					promise.m_FinishedCV.notify_all();

					// Wake the awaiting coroutine outside the lock: it re-enters this promise.
					if (continuation)
						continuation.resume();
				}

				void await_resume() const noexcept {}
			};

			auto final_suspend() const noexcept { return final_awaitable{}; }

			// False when the coroutine already finished: the caller must not suspend then.
			bool try_set_continuation(coro::coroutine_handle<> continuation)
			{
				std::lock_guard lock(m_ContinuationMutex);
				if (m_Finished)
					return false;

				m_Continuation = continuation;
				return true;
			}

			bool is_finished() const
			{
				std::lock_guard lock(m_ContinuationMutex);
				return m_Finished;
			}

			void wait_until_finished()
			{
				std::unique_lock lock(m_ContinuationMutex);
				m_FinishedCV.wait(lock, [this] { return m_Finished; });
			}

		private:
			mutable std::mutex m_ContinuationMutex;
			std::condition_variable m_FinishedCV;
			bool m_Finished = false;
			coro::coroutine_handle<> m_Continuation;
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
				return coro::coroutine_handle<promise_type>::from_promise(*this);
			}

			bool is_ready() const { return m_Result.has_value(); }
			const T& value() const { return m_Result.value(); }

		private:
			std::optional<T> m_Result;
		};

		template<>
		struct promise_type<void> : promise_type_base<void>
		{
			void return_void() { m_IsReady = true; }

			eager_task<void> get_return_object();

			bool is_ready() const { return m_IsReady; }
			void value() const { assert(m_IsReady); }

		private:
			bool m_IsReady = false;
		};
	}

	template<typename T = void>
	class eager_task
	{
	public:
		using promise_type = detail::eager_task_hpp::promise_type<T>;

		eager_task(detail::coro::coroutine_handle<promise_type> handle) : m_Handle(handle) {}

		bool await_ready() const
		{
			using namespace detail::eager_task_hpp;
			return m_Handle.promise().is_finished() ? AWAIT_READY_RESUME : AWAIT_READY_DEFER;
		}

		bool await_suspend(detail::coro::coroutine_handle<> handle)
		{
			using namespace detail::eager_task_hpp;
			// Park this coroutine until the task finishes, unless it already has.
			return m_Handle.promise().try_set_continuation(handle) ? AWAIT_SUSPEND_DEFER : AWAIT_SUSPEND_RESUME;
		}

		T await_resume()
		{
			m_Handle.promise().wait_until_finished();
			assert(m_Handle.promise().is_ready());
			assert(m_Handle.done());

			return m_Handle.promise().value();
		}

	private:
		detail::coro::coroutine_handle<promise_type> m_Handle;
	};

	inline eager_task<void> detail::eager_task_hpp::promise_type<void>::get_return_object()
	{
		return detail::coro::coroutine_handle<promise_type>::from_promise(*this);
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

#endif
