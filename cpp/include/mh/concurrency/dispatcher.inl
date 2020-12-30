#ifdef MH_COMPILE_LIBRARY
#include "dispatcher.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef MH_COROUTINES_SUPPORTED

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace mh
{
	namespace detail::dispatcher_hpp
	{
		struct task_data
		{
			mutable std::mutex m_TaskCompleteCVMutex;
			mutable std::condition_variable m_TaskCompleteCV;
			std::atomic<bool> m_IsTaskComplete = false;

			coro::coroutine_handle<> m_Handle;
		};

		struct thread_data
		{
			thread_data(bool singleThread) :
				m_IsSingleThread(singleThread)
			{
			}

			bool m_IsSingleThread{};

			mutable std::mutex m_TasksMutex;
			std::condition_variable m_TasksAvailableCV;
			std::queue<coro::coroutine_handle<>> m_Tasks;

			const std::thread::id m_OwnerThread = std::this_thread::get_id();
		};

		MH_COMPILE_LIBRARY_INLINE co_dispatch_task::co_dispatch_task(std::shared_ptr<thread_data> threadData) noexcept :
			m_ThreadData(std::move(threadData))
		{
		}

		MH_COMPILE_LIBRARY_INLINE bool co_dispatch_task::await_ready() const
		{
			return m_ThreadData->m_IsSingleThread && std::this_thread::get_id() == m_ThreadData->m_OwnerThread;
		}

		MH_COMPILE_LIBRARY_INLINE void co_dispatch_task::await_resume() const
		{
			assert(!m_ThreadData->m_IsSingleThread || m_ThreadData->m_OwnerThread == std::this_thread::get_id());
			//assert(m_TaskData);
			//std::unique_lock lock(m_TaskData->m_TaskCompleteCVMutex);
			//m_TaskData->m_TaskCompleteCV.wait(lock, [&] { return !m_TaskData->m_IsTaskComplete; });
		}

		MH_COMPILE_LIBRARY_INLINE bool co_dispatch_task::await_suspend(coro::coroutine_handle<> handle)
		{
			// Should never hit this, await_ready() should prevent suspension of coroutines
			// that are already on the correct thread (unless we are not single threaded, in which case we *want*
			// to be able to defer
			assert(!m_ThreadData->m_IsSingleThread || std::this_thread::get_id() != m_ThreadData->m_OwnerThread);

			{
				std::lock_guard lock(m_ThreadData->m_TasksMutex);
				m_ThreadData->m_Tasks.push(handle);
			}

			m_ThreadData->m_TasksAvailableCV.notify_one();

			return true;  // always suspend
		}
	}

	MH_COMPILE_LIBRARY_INLINE dispatcher::dispatcher(bool singleThread) :
		m_ThreadData(std::make_shared<thread_data>(singleThread))
	{
	}

	MH_COMPILE_LIBRARY_INLINE size_t dispatcher::run()
	{
		size_t count = 0;

		while (run_one())
			count++;

		return count;
	}

	MH_COMPILE_LIBRARY_INLINE bool dispatcher::run_one()
	{
		{
			const bool isAllowed = !m_ThreadData->m_IsSingleThread || m_ThreadData->m_OwnerThread == std::this_thread::get_id();
			assert(isAllowed);
			if (!isAllowed)
				return false;
		}

		using detail::dispatcher_hpp::task_data;

		if (!m_ThreadData->m_Tasks.empty())
		{
			mh::detail::coro::coroutine_handle<> task;
			{
				std::lock_guard lock(m_ThreadData->m_TasksMutex);
				if (m_ThreadData->m_Tasks.empty())
					return false; // Someone else grabbed the task out from under us

				task = std::move(m_ThreadData->m_Tasks.front());
				m_ThreadData->m_Tasks.pop();
			}

			// This could throw (...can it? what about promise_type::unhandled_exception()?)
			task.resume();
			return true;
		}

		return false;
	}

	MH_COMPILE_LIBRARY_INLINE detail::dispatcher_hpp::co_dispatch_task dispatcher::co_dispatch()
	{
		assert(m_ThreadData);
		return { m_ThreadData };
	}

	MH_COMPILE_LIBRARY_INLINE size_t dispatcher::task_count() const
	{
		return m_ThreadData->m_Tasks.size();
	}

	MH_COMPILE_LIBRARY_INLINE bool dispatcher::wait_tasks_for(std::chrono::high_resolution_clock::duration duration) const
	{
		auto threadData = m_ThreadData;
		std::unique_lock lock(threadData->m_TasksMutex);
		return threadData->m_TasksAvailableCV.wait_for(lock, duration, [&]() { return !threadData->m_Tasks.empty(); });
	}
}

#endif
