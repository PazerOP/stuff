#pragma once

#include "coroutine_include.hpp"

#ifdef MH_COROUTINES_SUPPORTED

#include "../concurrency/mutex_debug.hpp"
#include "../data/variable_pusher.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <future>
#include <variant>

namespace mh
{
	template<typename T = void> class task;

	enum class task_state
	{
		empty, // no state, never initialized, or was moved from
		running,
		value,
		exception,
	};

	namespace detail::task_hpp
	{
		template<typename T>
		struct co_promise_traits
		{
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using storage_type = T;
		};

		template<>
		struct co_promise_traits<void>
		{
			using value_type = void;
			using reference = void;
			using const_reference = void;
			using storage_type = std::monostate;
		};

		template<typename T>
		struct co_promise_traits<T&>
		{
			using value_type = T&;
			using reference = T&;
			using const_reference = const T&;
			using storage_type = std::reference_wrapper<std::remove_reference_t<T>>;
		};

		struct suspend_sometimes
		{
			constexpr suspend_sometimes(bool suspend) : m_Suspend(suspend) {}

			[[nodiscard]] constexpr bool await_ready() const noexcept
			{
				return !m_Suspend;
			}

			constexpr void await_suspend(coro::coroutine_handle<>) const noexcept {}
			constexpr void await_resume() const noexcept {}

			bool m_Suspend;
		};

		template<typename T>
		class promise_base : public co_promise_traits<T>
		{
			using traits = co_promise_traits<T>;
			using storage_type = typename traits::storage_type;

		public:
			~promise_base()
			{
#if defined(_DEBUG) && defined(_MSC_VER)
				if (m_BreakOnDestruct)
				{
					//auto test = reinterpret_cast<std::shared_ptr<const _EXCEPTION_DATA*>>(exception_ptr);
					//__debugbreak();
				}
#endif

				assert(m_RefCount == 0);
				m_Deleted = true;
			}

			static constexpr size_t IDX_WAITERS = 0;
			static constexpr size_t IDX_INVALID = 1;
			static constexpr size_t IDX_VALUE = 2;
			static constexpr size_t IDX_EXCEPTION = 3;

			constexpr task<T> get_return_object();

			bool is_ready() const noexcept
			{
				auto index = m_State.index();
				return index == IDX_VALUE || index == IDX_EXCEPTION;
			}

			bool valid() const noexcept
			{
				return m_State.index() != IDX_INVALID;
			}

			std::exception_ptr get_exception() const noexcept
			{
				auto result = std::get_if<IDX_EXCEPTION>(&m_State);
				return result ? *result : nullptr;
			}

			task_state get_task_state() const
			{
				switch (m_State.index())
				{
				case IDX_INVALID:    return task_state::empty;
				case IDX_WAITERS:    return task_state::running;
				case IDX_VALUE:      return task_state::value;
				case IDX_EXCEPTION:  return task_state::exception;
				}

				throw std::logic_error("Invalid state in mh::detail::task_hpp::promise_base<T>::get_task_state()");
			}

			void wait() const
			{
				if (!valid())
					throw std::future_error(std::future_errc::no_state);

				if (!is_ready())
				{
					std::unique_lock lock(m_Mutex);
					m_ValueReadyCV.wait(lock, [&] { return is_ready(); });
					assert(is_ready());
				}
			}
			template<typename Rep, typename Period>
			std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const
			{
				if (!valid())
					throw std::future_error(std::future_errc::no_state);

				std::unique_lock lock(m_Mutex);
				if (is_ready())
					return std::future_status::ready;

				if (!m_ValueReadyCV.wait_for(lock, timeout_duration, [&] { return is_ready(); }))
					return std::future_status::timeout;

				assert(is_ready());
				return std::future_status::ready;
			}
			template<typename Clock, typename Period>
			std::future_status wait_until(const std::chrono::time_point<Clock, Period>& timeout_time) const
			{
				if (!valid())
					throw std::future_error(std::future_errc::no_state);

				std::unique_lock lock(m_Mutex);
				if (is_ready())
					return std::future_status::ready;

				if (!m_ValueReadyCV.wait_until(lock, timeout_time, [&] { return is_ready(); }))
					return std::future_status::timeout;

				assert(is_ready());
				return std::future_status::ready;
			}

			void rethrow_if_exception() const
			{
				std::lock_guard lock(m_Mutex);
				if (auto ex = std::get_if<IDX_EXCEPTION>(&m_State))
				{
					m_BreakOnDestruct = true;
					std::rethrow_exception(*ex);
				}
			}

			T take_value()
			{
				wait();
				rethrow_if_exception();

				if constexpr (!std::is_void_v<T>)
				{
					auto value = std::move(std::get<IDX_VALUE>(m_State));
					m_State.template emplace<IDX_INVALID>();
					return std::move(value);
				}
			}

			void unhandled_exception()
			{
				set_state<IDX_EXCEPTION>(std::current_exception());
			}

			constexpr coro::suspend_never initial_suspend() const noexcept { return {}; }
			constexpr suspend_sometimes final_suspend() const noexcept
			{
#ifdef _DEBUG
				m_FinalSuspendHasRun = true;
#endif
				// If m_RefCount == 0, we are in charge of our own destiny (all referencing tasks have gone out of
				// scope, so just delete ourselves when we're done)
				return m_RefCount != 0;
			}

			bool await_ready() const { return is_ready(); }
			bool await_suspend(coro::coroutine_handle<> parent)
			{
				if (is_ready())
				{
					return false;
				}
				else
				{
					std::lock_guard lock(m_Mutex);
					if (is_ready())
					{
						return false;
					}
					else
					{
						std::get<IDX_WAITERS>(m_State).push_back(parent);
						return true; // suspend
					}
				}
			}

			template<size_t IDX, typename TValue>
			void set_state(TValue&& value)
			{
				//std::vector<coro::coroutine_handle<>> waiters;

				//{
					std::lock_guard lock(m_Mutex);

					if (is_ready())
						throw std::future_error(std::future_errc::promise_already_satisfied);

					auto waiters = std::move(std::get<IDX_WAITERS>(m_State));

					static_assert(IDX == IDX_VALUE || IDX == IDX_EXCEPTION);
					m_State.template emplace<IDX>(std::move(value));

					m_ValueReadyCV.notify_all();
				//}

#ifdef _DEBUG
				//mh::variable_pusher breakOnRemoveRef(m_BreakOnRemoveRef, true);
				[[maybe_unused]] auto self = coro::coroutine_handle<promise_base<T>>::from_promise(*this);
#endif

				for (auto& waiter : waiters)
				{
					//assert(m_RefCount > 0);
					waiter.resume();
					//assert(m_RefCount > 0);
				}
			}

			const storage_type* try_get_value() const { return std::get_if<IDX_VALUE>(&m_State); }
			storage_type* try_get_value() { return std::get_if<IDX_VALUE>(&m_State); }

			void add_ref()
			{
				assert(m_RefCount >= 1);
				++m_RefCount;
			}
			[[nodiscard]] bool remove_ref()
			{
#if defined(_DEBUG) && defined(_MSC_VER)
				if (m_BreakOnRemoveRef)
					__debugbreak();
#endif

				auto newVal = --m_RefCount;
				assert(newVal >= 0);
				return newVal <= 0;
			}

		protected:
			std::atomic_int32_t m_RefCount = 1;
			mutable mh::mutex_debug<> m_Mutex;
			mutable std::condition_variable_any m_ValueReadyCV;
			std::variant<std::vector<coro::coroutine_handle<>>, std::monostate, storage_type, std::exception_ptr> m_State;

#ifdef _DEBUG
			mutable bool m_BreakOnDestruct = false;
			bool m_BreakOnRemoveRef = false;
			mutable bool m_FinalSuspendHasRun = false;
			mutable bool m_Deleted = false;
#endif
		};
	}

	namespace detail
	{
		template<typename T>
		class promise final : public task_hpp::promise_base<T>
		{
			using super = detail::task_hpp::promise_base<T>;

		public:
			~promise() {}

			decltype(auto) get_value() { return const_cast<T&>(std::as_const(*this).get_value()); }
			decltype(auto) get_value() const
			{
				this->wait();

				this->rethrow_if_exception();

				return std::get<super::IDX_VALUE>(this->m_State);
			}

			T* try_get_value() { return const_cast<T*>(std::as_const(*this).try_get_value()); }
			const T* try_get_value() const
			{
				return std::get_if<super::IDX_VALUE>(std::addressof(this->m_State));
			}

			void return_value(T value)
			{
				this->set_state<super::IDX_VALUE>(std::move(value));
			}

			const T& await_resume() const { return get_value(); }
			T& await_resume() { return get_value(); }
		};

		template<>
		class promise<void> final : public task_hpp::promise_base<void>
		{
			using super = detail::task_hpp::promise_base<void>;

		public:
			~promise() {}

			void return_void()
			{
				this->set_state<super::IDX_VALUE>(std::monostate{});
			}

			void await_resume()
			{
				assert(is_ready());
				rethrow_if_exception();
			}
		};
	}

	namespace detail::task_hpp
	{
		template<typename T>
		class task_base
		{
		public:
			using promise_type = promise<T>;
			using coroutine_type = coro::coroutine_handle<promise_type>;

			task_base() noexcept = default;
			task_base(std::nullptr_t) noexcept : m_Handle(nullptr) {}
			task_base(coroutine_type state) noexcept : m_Handle(std::move(state)) {}

			task_base(const task_base& other) noexcept :
				m_Handle(other.m_Handle)
			{
				if (m_Handle)
					m_Handle.promise().add_ref();
			}
			task_base& operator=(const task_base& other) noexcept
			{
				release();
				m_Handle = other.m_Handle;
				if (m_Handle)
					m_Handle.promise().add_ref();

				return *this;
			}

			task_base(task_base&& other) noexcept :
				m_Handle(std::exchange(other.m_Handle, nullptr))
			{
				assert(std::addressof(other) != this);
			}
			task_base& operator=(task_base&& other) noexcept
			{
				assert(std::addressof(other) != this);
				release();
				m_Handle = std::exchange(other.m_Handle, nullptr);
				return *this;
			}

			~task_base()
			{
				release();
			}

			task_state state() const { return m_Handle ? m_Handle.promise().get_task_state() : task_state::empty; }

			operator bool() const { return valid(); }
			[[nodiscard]] bool valid() const { return m_Handle && m_Handle.promise().valid(); }
			[[nodiscard]] bool is_ready() const { return m_Handle && m_Handle.promise().is_ready(); }
			[[nodiscard]] bool empty() const { return !m_Handle; }

			void wait() const
			{
				return get_promise().wait();
			}
			template<typename Rep, typename Period>
			std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const
			{
				return get_promise().wait_for(timeout_duration);
			}
			template<typename Clock, typename Period>
			std::future_status wait_until(const std::chrono::time_point<Clock, Period>& timeout_time) const
			{
				return get_promise().wait_until(timeout_time);
			}

			std::exception_ptr get_exception() const { m_Handle ? m_Handle.promise().get_exception() : nullptr; }

			promise_type& operator co_await() { return get_promise(); }
			const promise_type& operator co_await() const { return get_promise(); }

			// TEMP: public
		//protected:
			promise_type& get_promise() { return const_cast<promise_type&>(std::as_const(*this).get_promise()); }
			const promise_type& get_promise() const
			{
				if (!m_Handle)
					throw std::future_error(std::future_errc::no_state);

				return m_Handle.promise();
			}

			coroutine_type m_Handle;

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
		};
	}

	template<typename T>
	class task final : public detail::task_hpp::task_base<T>
	{
		using super = detail::task_hpp::task_base<T>;

	public:
		using super::super;
		~task() {}

		const T& get() const { return this->get_promise().get_value(); }
		T& get() { return this->get_promise().get_value(); }

		const T* try_get() const { return this->m_Handle ? this->m_Handle.promise().try_get_value() : nullptr; }
		T* try_get() { return const_cast<T*>(std::as_const(*this).try_get()); }
	};

	template<>
	class task<void> final : public detail::task_hpp::task_base<void>
	{
		using super = detail::task_hpp::task_base<void>;

	public:
		using super::super;
		~task() {}
	};

	template<typename T>
	inline constexpr task<T> detail::task_hpp::promise_base<T>::get_return_object()
	{
		return { coro::coroutine_handle<detail::promise<T>>::from_promise(*static_cast<promise<T>*>(this)) };
	}

	template<typename T, typename... TArgs>
	inline task<T> make_ready_task(TArgs&&... args)
	{
		co_return T(std::forward<TArgs>(args)...);
	}
}

#endif
