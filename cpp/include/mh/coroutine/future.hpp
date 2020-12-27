#pragma once

#ifdef MH_COROUTINES_SUPPORTED

#include "coroutine_common.hpp"

#include <cassert>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <future>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

namespace mh
{
	template<typename T> class future;

	namespace detail::future_hpp
	{
		template<typename T>
		struct future_state : co_promise_base<T>
		{
		};

		template<typename T>
		class future_base : public co_promise_traits<T>
		{
			using state_t = future_state<T>;
			using state_ptr = std::shared_ptr<state_t>;

		public:
			future_base() = default;
			future_base(state_ptr state) : m_State(std::move(state)) {}

			future_base(const future_base&) = default;
			future_base& operator=(const future_base&) = default;

			future_base(future_base&&) noexcept = default;
			future_base& operator=(future_base&&) noexcept = default;

			bool is_ready() const { return m_State && m_State->is_ready(); }
			bool valid() const { return m_State && m_State->valid(); }

			template<typename TFunc>
			auto then(TFunc&& func)
			{
				//using continuation_t = std::invoke_result_t<TFunc,
				throw std::runtime_error("Not implemented");
			}

			void wait() const { return get_state().wait(); }

			template<typename Rep, typename Period>
			std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const
			{
				return get_state().wait_for(timeout_duration);
			}
			template<typename Clock, typename Period>
			std::future_status wait_until(const std::chrono::time_point<Clock, Period>& timeout_time) const
			{
				return get_state().wait_until(timeout_time);
			}

			const state_t& operator co_await() const { return get_state(); }
			state_t& operator co_await() { return get_state(); }

		protected:
			const future_state<T>& get_state() const
			{
				if (!m_State)
					throw std::future_error(std::future_errc::no_state);

				return *m_State;
			}
			future_state<T>& get_state()
			{
				return const_cast<future_state<T>&>(const_cast<const future_base<T>*>(this)->get_state());
			}

			state_ptr m_State;
		};

		template<typename T>
		class promise_base
		{
			using state_t = detail::future_hpp::future_state<T>;
			using state_ptr = std::shared_ptr<state_t>;
		public:
			promise_base() = default;
			promise_base(const promise_base&) = delete;
			promise_base(promise_base&&) noexcept = default;
			promise_base& operator=(const promise_base&) = delete;
			promise_base& operator=(promise_base&&) noexcept = default;

			future<T> get_future()
			{
				if (!m_State)
					throw std::future_error(std::future_errc::no_state);
				if (m_FutureRetrieved)
					throw std::future_error(std::future_errc::future_already_retrieved);

				m_FutureRetrieved = true;
				return future<T>(m_State);
			}

			void set_exception(std::exception_ptr p)
			{
				return get_state().set_state<state_t::IDX_EXCEPTION>(std::move(p));
			}

		protected:
			const state_t& get_state() const
			{
				if (!m_State)
					throw std::future_error(std::future_errc::no_state);

				return *m_State;
			}
			state_t& get_state()
			{
				return const_cast<state_t&>(const_cast<const promise_base*>(this)->get_state());
			}

			template<typename TValue>
			void try_set_value(TValue&& value)
			{
				return get_state().set_state<state_t::IDX_VALUE>(std::move(value));
			}

		private:
			state_ptr m_State = std::make_shared<state_t>();
			bool m_FutureRetrieved = false;
		};
	}

	template<typename T> class shared_future;

	template<typename T>
	class future : public detail::future_hpp::future_base<T>
	{
		using super = detail::future_hpp::future_base<T>;
	public:
		using super::super;

		future(future&&) noexcept = default;
		future& operator=(future&&) noexcept = default;

		// No copying
		future(const future&) = delete;
		future& operator=(const future&) = delete;

		shared_future<T> share();

		T get()
		{
			auto value = super::get_state().take_value();
			super::m_State.reset();
			return std::move(value);
		}
		T await_resume() { return get(); }
	};

	template<typename T>
	class promise : public detail::future_hpp::promise_base<T>
	{
		using super = detail::future_hpp::promise_base<T>;
	public:
		using super::super;
		void set_value(const T& value)
		{
			super::try_set_value(value);
		}
		void set_value(T&& value)
		{
			super::try_set_value(std::move(value));
		}
	};
	template<typename T>
	class promise<T&> : public detail::future_hpp::promise_base<T&>
	{
		using super = detail::future_hpp::promise_base<T&>;
	public:
		using super::super;
		void set_value(T& value)
		{
			super::try_set_value(&value);
		}
	};
	template<>
	class promise<void> : public detail::future_hpp::promise_base<void>
	{
		using super = detail::future_hpp::promise_base<void>;
	public:
		using super::super;
		void set_value()
		{
			super::try_set_value(std::monostate{});
		}
	};

	template<typename T>
	class shared_future : public detail::future_hpp::future_base<T>
	{
		using super = detail::future_hpp::future_base<T>;
	public:
		using super::super;
		shared_future(future<T>&& f) : shared_future(f.share()) {}

		shared_future(shared_future&&) noexcept = default;
		shared_future& operator=(shared_future&&) noexcept = default;

		shared_future(const shared_future&) = default;
		shared_future& operator=(const shared_future&) = default;

		typename super::const_reference get() const { return super::get_state().get_ref(); }
		typename super::const_reference await_resume() const { return get(); }
	};

	template<typename T>
	shared_future<T> future<T>::share()
	{
		return shared_future<T>(std::move(super::m_State));
	}
}

#endif
