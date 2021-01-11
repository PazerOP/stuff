#pragma once

#include "coroutine_common.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include <atomic>
#include <cassert>
#include <stdexcept>

namespace mh
{
	template<typename T = void> class task;

	namespace detail::task_hpp
	{
		struct suspend_sometimes
		{
			constexpr suspend_sometimes(bool suspend) : m_Suspend(suspend) {}

			[[nodiscard]] constexpr bool await_ready() const noexcept { return !m_Suspend; }

			constexpr void await_suspend(coro::coroutine_handle<>) const noexcept {}
			constexpr void await_resume() const noexcept {}

			bool m_Suspend;
		};

		template<typename T>
		struct promise : public co_promise_base<T>
		{
			constexpr coro::suspend_never initial_suspend() const noexcept { return {}; }
			constexpr suspend_sometimes final_suspend() const noexcept
			{
				// If m_RefCount == 0, we are in charge of our own destiny (all referencing tasks have gone out of
				// scope, so just delete ourselves when we're done)
				return m_RefCount != 0;
			}

			constexpr task<T> get_return_object();

			void add_ref()
			{
				assert(m_RefCount >= 1);
				++m_RefCount;
			}
			[[nodiscard]] bool remove_ref()
			{
				auto newVal = --m_RefCount;
				assert(newVal >= 0);
				return newVal <= 0;
			}

		private:
			std::atomic_int32_t m_RefCount = 1;
		};

		template<typename T>
		struct task_state
		{
			using promise_type = promise<T>;
			using coroutine_type = coro::coroutine_handle<promise_type>;

			task_state() noexcept = default;
			task_state(std::nullptr_t) noexcept : m_Handle(nullptr) {}
			task_state(coroutine_type handle) noexcept : m_Handle(std::move(handle)) {}

			task_state(const task_state& other) noexcept :
				m_Handle(other.m_Handle)
			{
				if (m_Handle)
					m_Handle.promise().add_ref();
			}
			task_state& operator=(const task_state& other) noexcept
			{
				release();
				m_Handle = other.m_Handle;
				if (m_Handle)
					m_Handle.promise().add_ref();

				return *this;
			}

			task_state(task_state&& other) noexcept :
				m_Handle(std::exchange(other.m_Handle, nullptr))
			{
				assert(std::addressof(other) != this);
			}
			task_state& operator=(task_state&& other) noexcept
			{
				assert(std::addressof(other) != this);
				release();
				m_Handle = std::exchange(other.m_Handle, nullptr);
				return *this;
			}

			~task_state()
			{
				release();
			}

			[[nodiscard]] bool empty() const { return !m_Handle; }

			const promise_type* try_get_promise() const { return m_Handle ? &m_Handle.promise() : nullptr; }
			promise_type* try_get_promise() { return const_cast<promise_type*>(const_cast<const task_state*>(this)->try_get_promise()); }

		private:
			void release()
			{
				if (m_Handle && m_Handle.promise().remove_ref())
				{
					// TODO probably a race condition here
					if (m_Handle.done())
						m_Handle.destroy();
				}

				m_Handle = nullptr;
			}

			coroutine_type m_Handle;
		};
	}

	template<typename T>
	class task : public awaitable<T, detail::task_hpp::task_state<T>>
	{
		using super = awaitable<T, detail::task_hpp::task_state<T>>;

	public:
		using super::super;
	};

	template<typename T>
	inline constexpr task<T> detail::task_hpp::promise<T>::get_return_object()
	{
		return { coro::coroutine_handle<detail::task_hpp::promise<T>>::from_promise(*this) };
	}

	template<typename T, typename... TArgs>
	inline task<T> make_ready_task(TArgs&&... args)
	{
		co_return T(std::forward<TArgs>(args)...);
	}

	namespace detail::task_hpp
	{
		constexpr bool has_co_await_operator(const void*) { return false; }

		template<typename T>
		constexpr auto has_co_await_operator(const T*) -> decltype(std::declval<T>().operator co_await(), bool{}) { return true; }

		template<typename T> inline constexpr bool has_co_await_operator_v = has_co_await_operator((T*)nullptr);

		template<typename T>
		using awaiter_t = std::conditional_t<has_co_await_operator_v<T>, decltype(std::declval<T>().operator co_await()), T>;

		template<typename T>
		using await_result_t = decltype(std::declval<awaiter_t<T>>().await_resume());
	}

	/// <summary>
	/// Allocates a callable in the coroutine promise so it is safe if we suspend.
	/// </summary>
	/// <returns>Result of co_awaiting the callable.</returns>
	template<typename TFunc, typename... TArgs>
	inline std::invoke_result_t<TFunc, TArgs...> make_lambda_task(TFunc func, TArgs... args)
	{
		co_return co_await func(std::forward<TArgs>(args)...);
	}
}

#endif
