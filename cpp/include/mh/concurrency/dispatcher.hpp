#pragma once

#if __has_include(<mh/coroutine/coroutine_include.hpp>)
#include <mh/coroutine/coroutine_include.hpp>
#endif

#ifdef MH_COROUTINES_SUPPORTED

#include <chrono>
#include <memory>

namespace mh
{
	namespace detail::dispatcher_hpp
	{
		struct thread_data;
		struct task_data;

		struct [[nodiscard]] co_dispatch_task
		{
			co_dispatch_task(std::shared_ptr<thread_data> threadData) noexcept;

			bool await_ready() const;
			void await_resume() const;
			bool await_suspend(coro::coroutine_handle<> parent);

		private:
			std::shared_ptr<thread_data> m_ThreadData;

			//std::shared_ptr<task_data> m_TaskData;
		};
	}

	class dispatcher
	{
		using thread_data = detail::dispatcher_hpp::thread_data;

	public:
		dispatcher(bool singleThread = true);

		size_t task_count() const;

		detail::dispatcher_hpp::co_dispatch_task co_dispatch();

		size_t run();
		bool run_one();
		template<typename TFunc>
		size_t run_while(TFunc&& func)
		{
			size_t count = 0;

			while (func() && run_one())
				count++;

			return count;
		}
		template<typename TClock, typename TDuration>
		size_t run_until(std::chrono::time_point<TClock, TDuration> endTime)
		{
			return run_while([&]()
				{
					auto now = TClock::now();
					return now < endTime;
				});
		}
		template<typename TRep, typename TPeriod>
		size_t run_for(std::chrono::duration<TRep, TPeriod> duration)
		{
			return run_until(std::chrono::high_resolution_clock::now() + duration);
		}

		void wait_tasks() const;
		bool wait_tasks_while(bool(*predicateFunc)(void* userData), void* userData = nullptr) const;
		bool wait_tasks_while(bool(*predicateFunc)(const void* userData), const void* userData = nullptr) const;
		bool wait_tasks_until(std::chrono::high_resolution_clock::time_point endTime) const;
		bool wait_tasks_for(std::chrono::high_resolution_clock::duration duration) const;

		template<typename TFunc>
		bool wait_tasks_while(TFunc&& func) const
		{
			using vp_func = std::conditional_t<std::is_const_v<TFunc>, const void*, void*>;

			bool (*predicateFunc)(vp_func* userData) = [](vp_func* userData)
			{
				return (*reinterpret_cast<TFunc*>(userData))();
			};
			return wait_tasks_while(predicateFunc, reinterpret_cast<vp_func>(&func));
		}

	private:
		std::shared_ptr<thread_data> m_ThreadData;
	};
}

#ifndef MH_COMPILE_LIBRARY
#include "dispatcher.inl"
#endif

#endif
