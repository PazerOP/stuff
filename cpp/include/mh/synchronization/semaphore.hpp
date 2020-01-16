#pragma once

#include <cstddef>

#if __has_include(<version>)
#include <version>
#endif

#if __cpp_lib_semaphore >= 201907
#include <semaphore>
#else
#include <condition_variable>
#include <mutex>
#endif

namespace mh
{
#if __cpp_lib_semaphore >= 201907
	template<std::ptrdiff_t LeastMaxValue = std::numeric_limits<std::ptrdiff_t>::max()>
		using counting_semaphore = std::counting_semapohre<LeastMaxValue>;
	using binary_semaphore = std::binary_semaphore;
#else
	template<std::ptrdiff_t LeastMaxValue = std::numeric_limits<std::ptrdiff_t>::max()>
	class counting_semaphore
	{
	public:
		constexpr explicit counting_semaphore(std::ptrdiff_t desired) : m_Counter(desired) {}
		counting_semaphore(const counting_semaphore&) = delete;
		counting_semaphore& operator=(const counting_semaphore&) = delete;
		counting_semaphore& operator=(counting_semaphore&&) = delete;

		static constexpr std::ptrdiff_t max() noexcept
		{
			if constexpr (LeastMaxValue == 1)
				return 1;
			else
				return std::numeric_limits<std::ptrdiff_t>::max();
		}

		void release(std::ptrdiff_t update = 1)
		{
			if (update < 0)
				throw std::invalid_argument("update must be >= 0");
			if (update > (max() - m_Counter))
				throw std::invalid_argument("value of update would exceed the max value for this semaphore");

			std::lock_guard lock(m_CVMutex);

			if (update == 1)
				m_Counter++;
			else
				m_Counter += update;

			m_CV.notify_all();
		}

		void acquire()
		{
			std::unique_lock lock(m_CVMutex);
			m_CV.wait(lock, [&]{ return m_Counter > 0; });
			m_Counter--;
		}

		bool try_acquire() noexcept
		{
			std::unique_lock lock(m_CVMutex, std::try_to_lock);
			if (!lock.owns_lock())
				return false;

			if (m_Counter < 1)
				return false;

			m_Counter--;
			return true;
		}

		template<class Rep, class Period>
		bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time)
		{
			if (!m_CVMutex.try_lock_for(rel_time))
				return false;

			std::unique_lock lock(m_CVMutex, std::adopt_lock);

			if (m_Counter < 1)
				return false;

			m_Counter--;
			return true;
		}

		template<class Clock, class Duration>
		bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time)
		{
			if (!m_CVMutex.try_lock_until(abs_time))
				return false;

			std::unique_lock lock(m_CVMutex, std::adopt_lock);

			if (m_Counter < 1)
				return false;

			m_Counter--;
			return true;
		}

	private:
		std::ptrdiff_t m_Counter;
		std::condition_variable_any m_CV;
		std::timed_mutex m_CVMutex;
	};

	using binary_semaphore = counting_semaphore<1>;
#endif
}
