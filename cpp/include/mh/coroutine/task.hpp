#pragma once

#ifdef MH_COROUTINES_SUPPORTED

#include "coroutine_common.hpp"

#include <atomic>
#include <cassert>
#include <coroutine>
#include <stdexcept>

namespace mh
{
	template<typename T = void> class task;

	namespace detail::task_hpp
	{
		template<typename T>
		struct promise : public co_promise_base<T>
		{
			constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
			constexpr std::suspend_always final_suspend() const noexcept { return {}; }

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
			using coroutine_type = std::coroutine_handle<promise_type>;

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

			const promise_type* try_get_promise() const { return m_Handle ? &m_Handle.promise() : nullptr; }
			promise_type* try_get_promise() { return const_cast<promise_type*>(const_cast<const task_state*>(this)->try_get_promise()); }

		private:
			void release()
			{
				if (m_Handle && m_Handle.promise().remove_ref())
					m_Handle.destroy();

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
		return { std::coroutine_handle<detail::task_hpp::promise<T>>::from_promise(*this) };
	}

	template<typename T, typename... TArgs>
	inline task<T> make_ready_task(TArgs&&... args)
	{
		co_return T(std::forward<TArgs>(args)...);
	}
}

#endif
