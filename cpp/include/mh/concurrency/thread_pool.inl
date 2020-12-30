#ifdef MH_COMPILE_LIBRARY
#include "thread_pool.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef MH_COROUTINES_SUPPORTED

namespace mh
{
	MH_COMPILE_LIBRARY_INLINE thread_pool::thread_pool(size_t threadCount)
	{
		if (threadCount < 1)
			throw std::invalid_argument("threadCount must be >= 1");

		for (size_t i = 0; i < threadCount; i++)
			m_ThreadData->m_Threads.push_back(std::thread(&ThreadFunc, m_ThreadData));
	}

	MH_COMPILE_LIBRARY_INLINE thread_pool::~thread_pool()
	{
		m_ThreadData->m_IsShuttingDown = true;
		for (auto& thread : m_ThreadData->m_Threads)
			thread.detach();
	}

	MH_COMPILE_LIBRARY_INLINE size_t thread_pool::thread_count() const
	{
		return m_ThreadData->m_Threads.size();
	}

	MH_COMPILE_LIBRARY_INLINE size_t thread_pool::task_count() const
	{
		return m_ThreadData->m_Dispatcher.task_count();
	}

	MH_COMPILE_LIBRARY_INLINE void thread_pool::ThreadFunc(std::shared_ptr<thread_data> data)
	{
		using namespace std::chrono_literals;

		while (!data->m_IsShuttingDown)
		{
			data->m_Dispatcher.wait_tasks_for(1s);

			if (!data->m_IsShuttingDown)
			{
				try
				{
					data->m_Dispatcher.run_for(1s);
				}
				catch (...)
				{
					assert(!"Theoretically we should never get here?");
				}
			}
		}
	}
}
#endif
