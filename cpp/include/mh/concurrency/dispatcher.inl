#ifdef MH_COMPILE_LIBRARY
#include "dispatcher.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef MH_COROUTINES_SUPPORTED

#include <mh/error/not_implemented_error.hpp>
#include <mh/containers/heap.hpp>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

// Platform-specific I/O monitoring (Unix only for now)
#ifndef _WIN32
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#undef min
#undef max

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

		struct task_delay_data
		{
			constexpr bool operator<(const task_delay_data &rhs) const
			{
				// intentionally reversed
				return rhs.m_DelayUntilTime < m_DelayUntilTime;
			}

			clock_t::time_point m_DelayUntilTime;
			coro::coroutine_handle<> m_Handle;
		};

		struct task_fd_data
		{
			int m_FD;
			coro::coroutine_handle<> m_Handle;
		};

		struct thread_data
		{
			thread_data(bool singleThread) : m_IsSingleThread(singleThread)
			{
			}

			coro::coroutine_handle<> try_pop_task()
			{
				// Check for ready FDs first
				auto ready_fd_tasks = check_fd_tasks();
				if (!ready_fd_tasks.empty())
				{
					if (ready_fd_tasks.size() > 1)
					{
						// Re-queue the other ready FD tasks so they are not lost
						std::lock_guard lock(m_TasksMutex);
						for (size_t i = 1; i < ready_fd_tasks.size(); i++)
							m_Tasks.push(ready_fd_tasks[i]);
						m_TasksAvailableCV.notify_all();
					}

					return ready_fd_tasks[0]; // Return first ready FD task
				}

				std::lock_guard lock(m_TasksMutex);
				if (!m_Tasks.empty() || !m_DelayTasks.empty())
				{
					if (!m_DelayTasks.empty())
					{
						auto now = clock_t::now();
						const task_delay_data &taskDelayData = m_DelayTasks.front();
						if (taskDelayData.m_DelayUntilTime <= now)
						{
							auto task = taskDelayData.m_Handle;
							m_DelayTasks.pop();
							return task;
						}
					}

					if (!m_Tasks.empty())
					{
						auto task = m_Tasks.front();
						m_Tasks.pop();
						return task;
					}
				}

				return nullptr;
			}

			void add_task(coro::coroutine_handle<> task)
			{
				std::lock_guard lock(m_TasksMutex);
				m_Tasks.push(task);
				m_TasksAvailableCV.notify_one();
			}
			void add_delay_task(task_delay_data data)
			{
				std::lock_guard lock(m_TasksMutex);
				m_DelayTasks.push(std::move(data));
			}

			bool wait_tasks_until(const clock_t::time_point endTime) const
			{
				std::unique_lock lock(m_TasksMutex);

				const auto IsTaskAvailable = [&]
				{
					if (!m_DelayTasks.empty() && m_DelayTasks.front().m_DelayUntilTime <= clock_t::now())
						return true;

					if (!m_Tasks.empty())
						return true;

					return false;
				};

				while (endTime > clock_t::now())
				{
					auto localEndTime = endTime;
					if (!m_DelayTasks.empty())
						localEndTime = std::min(localEndTime, m_DelayTasks.front().m_DelayUntilTime);

					if (m_TasksAvailableCV.wait_until(lock, localEndTime, IsTaskAvailable))
						return true;
				}

				return false;
			}

			void add_fd_read_task(task_fd_data data)
			{
				std::lock_guard lock(m_TasksMutex);
				m_ReadTasks.push_back(std::move(data));
			}
			void add_fd_write_task(task_fd_data data)
			{
				std::lock_guard lock(m_TasksMutex);
				m_WriteTasks.push_back(std::move(data));
			}

			// Check for ready FDs and return ready tasks
			std::vector<coro::coroutine_handle<>> check_fd_tasks()
			{
				std::vector<coro::coroutine_handle<>> ready_tasks;
				std::lock_guard lock(m_TasksMutex);

#ifdef _WIN32
				// Windows: stub implementation for now
				throw mh::not_implemented_error(MH_SOURCE_LOCATION_CURRENT());
#else
				// Unix: use select()
				if (!m_ReadTasks.empty() || !m_WriteTasks.empty())
				{
					fd_set read_set, write_set;
					FD_ZERO(&read_set);
					FD_ZERO(&write_set);
					int max_fd = -1;

					// Add read FDs
					for (const auto &task : m_ReadTasks)
					{
						FD_SET(task.m_FD, &read_set);
						max_fd = std::max(max_fd, task.m_FD);
					}

					// Add write FDs
					for (const auto &task : m_WriteTasks)
					{
						FD_SET(task.m_FD, &write_set);
						max_fd = std::max(max_fd, task.m_FD);
					}

					// Non-blocking select
					struct timeval timeout = {0, 0};
					int result = select(max_fd + 1, &read_set, &write_set, nullptr, &timeout);

					if (result > 0)
					{
						// Check ready read FDs
						auto read_it = m_ReadTasks.begin();
						while (read_it != m_ReadTasks.end())
						{
							if (FD_ISSET(read_it->m_FD, &read_set))
							{
								ready_tasks.push_back(read_it->m_Handle);
								read_it = m_ReadTasks.erase(read_it);
							}
							else
							{
								++read_it;
							}
						}

						// Check ready write FDs
						auto write_it = m_WriteTasks.begin();
						while (write_it != m_WriteTasks.end())
						{
							if (FD_ISSET(write_it->m_FD, &write_set))
							{
								ready_tasks.push_back(write_it->m_Handle);
								write_it = m_WriteTasks.erase(write_it);
							}
							else
							{
								++write_it;
							}
						}
					}
				}
#endif
				return ready_tasks;
			}

			size_t task_count() const { std::lock_guard lock(m_TasksMutex); return m_Tasks.size() + m_DelayTasks.size() + m_ReadTasks.size() + m_WriteTasks.size(); }

			bool m_IsSingleThread{};

			const std::thread::id m_OwnerThread = std::this_thread::get_id();

		private:
			mutable std::mutex m_TasksMutex;
			mutable std::condition_variable m_TasksAvailableCV;
			std::queue<coro::coroutine_handle<>> m_Tasks;
			mh::heap<task_delay_data> m_DelayTasks;
			std::vector<task_fd_data> m_ReadTasks;
			std::vector<task_fd_data> m_WriteTasks;
		};

		MH_COMPILE_LIBRARY_INLINE co_dispatch_task::co_dispatch_task(std::shared_ptr<thread_data> threadData) noexcept : m_ThreadData(std::move(threadData))
		{
		}

		MH_COMPILE_LIBRARY_INLINE bool co_dispatch_task::await_ready() const
		{
			return m_ThreadData->m_IsSingleThread && std::this_thread::get_id() == m_ThreadData->m_OwnerThread;
		}

		MH_COMPILE_LIBRARY_INLINE void co_dispatch_task::await_resume() const
		{
			assert(!m_ThreadData->m_IsSingleThread || m_ThreadData->m_OwnerThread == std::this_thread::get_id());
			// assert(m_TaskData);
			// std::unique_lock lock(m_TaskData->m_TaskCompleteCVMutex);
			// m_TaskData->m_TaskCompleteCV.wait(lock, [&] { return !m_TaskData->m_IsTaskComplete; });
		}

		MH_COMPILE_LIBRARY_INLINE bool co_dispatch_task::await_suspend(coro::coroutine_handle<> handle)
		{
			// Should never hit this, await_ready() should prevent suspension of coroutines
			// that are already on the correct thread (unless we are not single threaded, in which case we *want*
			// to be able to defer
			assert(!m_ThreadData->m_IsSingleThread || std::this_thread::get_id() != m_ThreadData->m_OwnerThread);

			m_ThreadData->add_task(handle);

			return true; // always suspend
		}

		MH_COMPILE_LIBRARY_INLINE co_delay_task::co_delay_task(
			std::shared_ptr<thread_data> threadData, clock_t::time_point delayUntilTime) noexcept : m_ThreadData(std::move(threadData)), m_DelayUntilTime(std::move(delayUntilTime))
		{
		}

		MH_COMPILE_LIBRARY_INLINE bool co_delay_task::await_ready() const
		{
			return m_DelayUntilTime <= clock_t::now();
		}
		MH_COMPILE_LIBRARY_INLINE void co_delay_task::await_resume() const
		{
			// throw mh::not_implemented_error();
		}
		MH_COMPILE_LIBRARY_INLINE bool co_delay_task::await_suspend(coro::coroutine_handle<> parent)
		{
			if (await_ready())
				return false; // no need for suspension

			{
				task_delay_data data;
				data.m_DelayUntilTime = m_DelayUntilTime;
				data.m_Handle = parent;
				m_ThreadData->add_delay_task(std::move(data));
			}

			return true; // suspend
		}

		MH_COMPILE_LIBRARY_INLINE co_fd_read_task::co_fd_read_task(
			std::shared_ptr<thread_data> threadData, int fd) noexcept : m_ThreadData(std::move(threadData)), m_FD(fd)
		{
		}

		MH_COMPILE_LIBRARY_INLINE bool co_fd_read_task::await_ready() const
		{
			// Always suspend for FD monitoring
			return false;
		}
		MH_COMPILE_LIBRARY_INLINE void co_fd_read_task::await_resume() const
		{
			// FD is ready for reading
		}
		MH_COMPILE_LIBRARY_INLINE bool co_fd_read_task::await_suspend(coro::coroutine_handle<> parent)
		{
			task_fd_data data;
			data.m_FD = m_FD;
			data.m_Handle = parent;
			m_ThreadData->add_fd_read_task(std::move(data));

			return true; // suspend
		}

		MH_COMPILE_LIBRARY_INLINE co_fd_write_task::co_fd_write_task(
			std::shared_ptr<thread_data> threadData, int fd) noexcept : m_ThreadData(std::move(threadData)), m_FD(fd)
		{
		}

		MH_COMPILE_LIBRARY_INLINE bool co_fd_write_task::await_ready() const
		{
			// Always suspend for FD monitoring
			return false;
		}
		MH_COMPILE_LIBRARY_INLINE void co_fd_write_task::await_resume() const
		{
			// FD is ready for writing
		}
		MH_COMPILE_LIBRARY_INLINE bool co_fd_write_task::await_suspend(coro::coroutine_handle<> parent)
		{
			task_fd_data data;
			data.m_FD = m_FD;
			data.m_Handle = parent;
			m_ThreadData->add_fd_write_task(std::move(data));

			return true; // suspend
		}
	}

	MH_COMPILE_LIBRARY_INLINE dispatcher::dispatcher(bool singleThread) : m_ThreadData(std::make_shared<thread_data>(singleThread))
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

		if (mh::detail::coro::coroutine_handle<> task = m_ThreadData->try_pop_task())
		{
			// This could throw (...can it? what about promise_type::unhandled_exception()?)
			task.resume();
			return true;
		}

		return false;
	}

	MH_COMPILE_LIBRARY_INLINE detail::dispatcher_hpp::co_dispatch_task dispatcher::co_dispatch()
	{
		assert(m_ThreadData);
		return {m_ThreadData};
	}

	MH_COMPILE_LIBRARY_INLINE size_t dispatcher::task_count() const
	{
		return m_ThreadData->task_count();
	}

	MH_COMPILE_LIBRARY_INLINE bool dispatcher::wait_tasks_for(clock_t::duration duration) const
	{
		return wait_tasks_until(clock_t::now() + duration);
	}
	MH_COMPILE_LIBRARY_INLINE bool dispatcher::wait_tasks_until(clock_t::time_point endTime) const
	{
		return m_ThreadData->wait_tasks_until(endTime);
	}

	MH_COMPILE_LIBRARY_INLINE detail::dispatcher_hpp::co_delay_task dispatcher::co_delay_for(clock_t::duration duration)
	{
		return co_delay_until(clock_t::now() + duration);
	}
	MH_COMPILE_LIBRARY_INLINE detail::dispatcher_hpp::co_delay_task dispatcher::co_delay_until(clock_t::time_point endTime)
	{
		return detail::dispatcher_hpp::co_delay_task(m_ThreadData, endTime);
	}

	MH_COMPILE_LIBRARY_INLINE detail::dispatcher_hpp::co_fd_read_task dispatcher::co_wait_fd_read(int fd)
	{
		return detail::dispatcher_hpp::co_fd_read_task(m_ThreadData, fd);
	}
	MH_COMPILE_LIBRARY_INLINE detail::dispatcher_hpp::co_fd_write_task dispatcher::co_wait_fd_write(int fd)
	{
		return detail::dispatcher_hpp::co_fd_write_task(m_ThreadData, fd);
	}

	// Static member definition
	MH_COMPILE_LIBRARY_INLINE thread_local dispatcher* dispatcher::s_current_thread_dispatcher = nullptr;

	MH_COMPILE_LIBRARY_INLINE void dispatcher::register_for_current_thread()
	{
		if (s_current_thread_dispatcher) {
			throw std::runtime_error("Dispatcher already registered for current thread");
		}
		s_current_thread_dispatcher = this;
	}

	MH_COMPILE_LIBRARY_INLINE dispatcher& dispatcher::get()
	{
		if (!s_current_thread_dispatcher) {
			throw std::runtime_error("No dispatcher registered for current thread");
		}
		return *s_current_thread_dispatcher;
	}

	MH_COMPILE_LIBRARY_INLINE dispatcher* dispatcher::try_get()
	{
		return s_current_thread_dispatcher;
	}
}

#endif
