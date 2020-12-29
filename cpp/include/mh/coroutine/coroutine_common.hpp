#pragma once

#ifdef MH_COROUTINES_SUPPORTED
#include <cassert>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <future>
#include <mutex>
#include <variant>
#include <vector>

namespace mh
{
	enum class task_state
	{
		empty, // no state, never initialized, or was moved from
		running,
		value,
		exception,
	};

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

	namespace detail::coroutine_common_hpp
	{
		template<typename T>
		class promise_base : public co_promise_traits<T>
		{
			using traits = co_promise_traits<T>;
			using storage_type = typename traits::storage_type;

		public:
			static constexpr size_t IDX_WAITERS = 0;
			static constexpr size_t IDX_INVALID = 1;
			static constexpr size_t IDX_VALUE = 2;
			static constexpr size_t IDX_EXCEPTION = 3;

			bool is_ready() const
			{
				auto index = m_State.index();
				return index == IDX_VALUE || index == IDX_EXCEPTION;
			}

			bool valid() const
			{
				return m_State.index() != IDX_INVALID;
			}

			std::exception_ptr get_exception() const
			{
				return std::get_if<IDX_EXCEPTION>(&m_State);
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
			}

			void wait() const
			{
				if (!is_ready())
				{
					if (!valid())
						throw std::future_error(std::future_errc::no_state);

					std::unique_lock lock(m_Mutex);
					m_ValueReadyCV.wait(lock, [&] { return is_ready(); });
					assert(is_ready());
				}
			}
			template<typename Rep, typename Period>
			std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const
			{
				std::unique_lock lock(m_Mutex);
				if (is_ready())
					return std::future_status::ready;

				if (!valid())
					throw std::future_error(std::future_errc::no_state);

				if (!m_ValueReadyCV.wait_for(lock, timeout_duration, [&] { return is_ready(); }))
					return std::future_status::timeout;

				assert(is_ready());
				return std::future_status::ready;
			}
			template<typename Clock, typename Period>
			std::future_status wait_until(const std::chrono::time_point<Clock, Period>& timeout_time) const
			{
				std::unique_lock lock(m_Mutex);
				if (is_ready())
					return std::future_status::ready;

				if (!valid())
					throw std::future_error(std::future_errc::no_state);

				if (!m_ValueReadyCV.wait_until(lock, timeout_time, [&] { return is_ready(); }))
					return std::future_status::timeout;

				assert(is_ready());
				return std::future_status::ready;
			}

			void rethrow_if_exception() const
			{
				if (auto ex = std::get_if<IDX_EXCEPTION>(&m_State))
					std::rethrow_exception(*ex);
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

			bool await_ready() const { return is_ready(); }
			bool await_suspend(std::coroutine_handle<> parent)
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
				std::lock_guard lock(m_Mutex);

				if (is_ready())
					throw std::future_error(std::future_errc::promise_already_satisfied);

				auto waiters = std::move(std::get<IDX_WAITERS>(m_State));

				static_assert(IDX == IDX_VALUE || IDX == IDX_EXCEPTION);
				m_State.template emplace<IDX>(std::move(value));

				m_ValueReadyCV.notify_all();
				for (const auto& waiter : waiters)
					waiter.resume();
			}

			const storage_type* try_get_value() const { return std::get_if<IDX_VALUE>(&m_State); }
			storage_type* try_get_value() { return std::get_if<IDX_VALUE>(&m_State); }

		protected:
			mutable std::mutex m_Mutex;
			mutable std::condition_variable m_ValueReadyCV;
			std::variant<std::vector<std::coroutine_handle<>>, std::monostate, storage_type, std::exception_ptr> m_State;
		};
	}

	template<typename T>
	class co_promise_base : public detail::coroutine_common_hpp::promise_base<T>
	{
		using super = detail::coroutine_common_hpp::promise_base<T>;

	public:
		decltype(auto) get_value() const
		{
			super::wait();

			super::rethrow_if_exception();

			return std::get<super::IDX_VALUE>(super::m_State);
		}

		decltype(auto) get_value() { return const_cast<T&>(const_cast<const co_promise_base*>(this)->get_value()); }

		const T* try_get_value() const
		{
			return std::get_if<super::IDX_VALUE>(std::addressof(super::m_State));
		}
		T* try_get_value() { return const_cast<T*>(const_cast<const co_promise_base*>(this)->try_get_ptr()); }

		void return_value(T value)
		{
			super::set_state<super::IDX_VALUE>(std::move(value));
		}

		const T& await_resume() const { return get_value(); }
		T& await_resume() { return get_value(); }
	};

	template<>
	class co_promise_base<void> : public detail::coroutine_common_hpp::promise_base<void>
	{
		using super = detail::coroutine_common_hpp::promise_base<void>;

	public:
		void return_void()
		{
			super::set_state<super::IDX_VALUE>(std::monostate{});
		}

		void await_resume()
		{
			assert(is_ready());
		}
	};

	namespace detail::coroutine_common_hpp
	{
		template<typename T, typename TState>
		class awaitable_base
		{
		public:
			using promise_type = typename TState::promise_type;

			awaitable_base() noexcept = default;
			awaitable_base(TState state) noexcept : m_State(std::move(state)) {}

			task_state state() const
			{
				auto promise = m_State.try_get_promise();
				return promise ? promise->state() : task_state::empty;
			}

			operator bool() const { return valid(); }
			bool valid() const
			{
				auto promise = m_State.try_get_promise();
				return promise && promise->valid();
			}

			bool is_ready() const
			{
				auto promise = m_State.try_get_promise();
				return promise && promise->is_ready();
			}

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

			std::exception_ptr get_exception() const
			{
				auto promise = m_State.try_get_promise();
				return promise ? promise->get_exception() : nullptr;
			}

			promise_type& operator co_await() { return get_promise(); }
			const promise_type& operator co_await() const { return get_promise(); }

		protected:
			const promise_type& get_promise() const
			{
				auto promise = m_State.try_get_promise();
				if (!promise || !promise->valid())
					throw std::future_error(std::future_errc::no_state);

				return *promise;
			}
			promise_type& get_promise() { return const_cast<promise_type&>(const_cast<const awaitable_base*>(this)->get_promise()); }

		protected:
			TState m_State;
		};
	}

	template<typename T, typename TState>
	class awaitable : public detail::coroutine_common_hpp::awaitable_base<T, TState>
	{
		using super = detail::coroutine_common_hpp::awaitable_base<T, TState>;

	public:
		using super::super;

		const T& get() const { return super::get_promise().get_value(); }
		T& get() { return super::get_promise().get_value(); }

		const T* try_get() const
		{
			auto promise = super::m_State.try_get_promise();
			if (!promise)
				return nullptr;

			return promise->try_get_value();
		}

		T* try_get()
		{
			auto promise = super::m_State.try_get_promise();
			if (!promise)
				return nullptr;

			return promise->try_get_value();
		}
	};

	template<typename TState>
	class awaitable<void, TState> : public detail::coroutine_common_hpp::awaitable_base<void, TState>
	{
		using super = detail::coroutine_common_hpp::awaitable_base<void, TState>;

	public:
		using super::super;
	};
}

#if __has_include(<mh/reflection/enum.hpp>)
#include <mh/reflection/enum.hpp>

MH_ENUM_REFLECT_BEGIN(mh::task_state)
	MH_ENUM_REFLECT_VALUE(empty)
	MH_ENUM_REFLECT_VALUE(running)
	MH_ENUM_REFLECT_VALUE(value)
	MH_ENUM_REFLECT_VALUE(exception)
MH_ENUM_REFLECT_END()
#endif

#endif
