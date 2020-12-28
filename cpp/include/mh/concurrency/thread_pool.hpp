#pragma once

#if __has_include(<mh/coroutine/future.hpp>)
#include <mh/coroutine/future.hpp>
#else
#include <future>
#endif

#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace mh
{
	template<typename T>
	class thread_pool final
	{
	public:
		using function_type = std::function<T()>;

#if __has_include(<mh/coroutine/future.hpp>)
		using promise_type = mh::promise<T>;
		using future_type = mh::shared_future<T>;
#else
		using promise_type = std::promise<T>;
		using future_type = std::shared_future<T>;
#endif

		thread_pool(size_t threadCount = std::thread::hardware_concurrency())
		{
			if (threadCount < 1)
				throw std::invalid_argument("threadCount must be >= 1");

			for (size_t i = 0; i < threadCount; i++)
				m_Threads.push_back(std::thread(&ThreadFunc, m_ThreadData));
		}
		~thread_pool()
		{
			m_ThreadData->m_IsShuttingDown = true;
			for (auto& thread : m_Threads)
				thread.detach();
		}

		future_type add_task(function_type func)
		{
			std::lock_guard lock(m_ThreadData->m_TasksMutex);

			Task& task = m_ThreadData->m_Tasks.emplace();
			task.m_Function = std::move(func);

			m_ThreadData->m_TasksCV.notify_one();

			return task.m_Promise.get_future();
		}

		size_t thread_count() const { return m_Threads.size(); }
		size_t task_count() const { return m_ThreadData->m_Tasks.size(); }

	private:
		struct Task
		{
			function_type m_Function;
			promise_type m_Promise;
		};

		struct ThreadData
		{
			bool m_IsShuttingDown = false;

			std::queue<Task> m_Tasks;
			mutable std::mutex m_TasksMutex;
			std::condition_variable m_TasksCV;
		};
		std::shared_ptr<ThreadData> m_ThreadData = std::make_shared<ThreadData>();
		std::vector<std::thread> m_Threads;

		static void ThreadFunc(std::shared_ptr<ThreadData> data)
		{
			using namespace std::chrono_literals;

			while (!data->m_IsShuttingDown)
			{
				{
					std::unique_lock lock(data->m_TasksMutex);
					data->m_TasksCV.wait_for(lock, 1s);
				}

				while (!data->m_Tasks.empty() && !data->m_IsShuttingDown)
				{
					Task task;
					{
						std::lock_guard lock(data->m_TasksMutex);
						if (data->m_Tasks.empty())
							break;

						task = std::move(data->m_Tasks.front());
						data->m_Tasks.pop();
					}

					try
					{
						if constexpr (std::is_same_v<T, void>)
						{
							task.m_Function();
							task.m_Promise.set_value();
						}
						else
						{
							task.m_Promise.set_value(task.m_Function());
						}
					}
					catch (...)
					{
						task.m_Promise.set_exception(std::current_exception());
					}
				}
			}
		}
	};
}
