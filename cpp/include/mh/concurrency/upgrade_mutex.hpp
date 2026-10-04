#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <system_error>

#ifndef MH_STUFF_API
#define MH_STUFF_API
#endif

namespace mh
{
	/**
	 * An upgradable shared mutex modeled on Boost.Thread's UpgradeLockable concept.
	 *
	 * Three ownership levels:
	 * - shared: any number of threads at once.
	 * - upgrade: at most ONE thread, coexists with shared owners. Holding upgrade
	 *   ownership reserves the exclusive right to later upgrade to exclusive
	 *   ownership without releasing the lock (two shared owners that both tried to
	 *   upgrade in-place would deadlock waiting for each other to release).
	 * - exclusive: one thread, excludes shared, upgrade, and exclusive.
	 *
	 * Invariants/semantics:
	 * - unlock_upgrade_and_lock() marks an upgrade transition as pending, then waits
	 *   for existing shared owners to drain. From that instant, new lock_shared()
	 *   callers block and try_lock_shared() fails until the upgrader has taken (and
	 *   released) its exclusive hold; lock_upgrade()/try_lock_upgrade() likewise.
	 * - The transition is non-preemptible: upgrade ownership is retained until the
	 *   conversion completes, so no lock() caller can take exclusive ownership
	 *   between the drain and the conversion.
	 * - Only the upgrade transition has priority over new shared acquires. Plain
	 *   lock() has no such priority and can starve under continuous shared load.
	 * - Downgrades (exclusive->upgrade, exclusive->shared, upgrade->shared) never
	 *   block.
	 * - No recursion. Unlocking a mode the calling thread does not hold is
	 *   undefined behavior.
	 *
	 * Meets the standard SharedLockable requirements, so std::shared_lock and
	 * std::unique_lock work unmodified. mh::upgrade_lock is the RAII guard for
	 * upgrade ownership, and mh::upgrade_to_unique_lock is a scoped upgrade.
	 */
	class upgrade_mutex
	{
	public:
		upgrade_mutex() = default;
		upgrade_mutex(const upgrade_mutex&) = delete;
		upgrade_mutex& operator=(const upgrade_mutex&) = delete;

		// Exclusive ownership
		MH_STUFF_API void lock();
		MH_STUFF_API bool try_lock();
		MH_STUFF_API void unlock();

		// Shared ownership
		MH_STUFF_API void lock_shared();
		MH_STUFF_API bool try_lock_shared();
		MH_STUFF_API void unlock_shared();

		// Upgrade ownership
		MH_STUFF_API void lock_upgrade();
		MH_STUFF_API bool try_lock_upgrade();
		MH_STUFF_API void unlock_upgrade();

		// Upgrade ownership -> exclusive ownership. Blocks new shared/upgrade
		// acquires immediately, waits for existing shared owners to drain.
		MH_STUFF_API void unlock_upgrade_and_lock();

		// Downgrades. Never block.
		MH_STUFF_API void unlock_and_lock_upgrade();
		MH_STUFF_API void unlock_and_lock_shared();
		MH_STUFF_API void unlock_upgrade_and_lock_shared();

	private:
		std::mutex m_Mutex;
		std::condition_variable m_CV;
		size_t m_SharedCount = 0;
		bool m_HasUpgrade = false;
		bool m_HasExclusive = false;
		bool m_UpgradePending = false;
	};

	/**
	 * RAII guard for upgrade ownership of an UpgradeLockable mutex. Mirrors the
	 * shape of std::shared_lock, but calls lock_upgrade()/try_lock_upgrade()/
	 * unlock_upgrade().
	 */
	template<typename Mutex>
	class upgrade_lock
	{
	public:
		using mutex_type = Mutex;

		upgrade_lock() noexcept = default;
		explicit upgrade_lock(mutex_type& mutex) : m_Mutex(&mutex), m_Owns(true) { mutex.lock_upgrade(); }
		upgrade_lock(mutex_type& mutex, std::defer_lock_t) noexcept : m_Mutex(&mutex), m_Owns(false) {}
		upgrade_lock(mutex_type& mutex, std::try_to_lock_t) : m_Mutex(&mutex), m_Owns(mutex.try_lock_upgrade()) {}
		upgrade_lock(mutex_type& mutex, std::adopt_lock_t) noexcept : m_Mutex(&mutex), m_Owns(true) {}

		~upgrade_lock()
		{
			if (m_Owns)
				m_Mutex->unlock_upgrade();
		}

		upgrade_lock(const upgrade_lock&) = delete;
		upgrade_lock& operator=(const upgrade_lock&) = delete;

		upgrade_lock(upgrade_lock&& other) noexcept : m_Mutex(other.m_Mutex), m_Owns(other.m_Owns)
		{
			other.m_Mutex = nullptr;
			other.m_Owns = false;
		}
		upgrade_lock& operator=(upgrade_lock&& other) noexcept
		{
			if (m_Owns)
				m_Mutex->unlock_upgrade();

			m_Mutex = other.m_Mutex;
			m_Owns = other.m_Owns;
			other.m_Mutex = nullptr;
			other.m_Owns = false;
			return *this;
		}

		void lock()
		{
			check_lockable();
			m_Mutex->lock_upgrade();
			m_Owns = true;
		}
		bool try_lock()
		{
			check_lockable();
			m_Owns = m_Mutex->try_lock_upgrade();
			return m_Owns;
		}
		void unlock()
		{
			if (!m_Owns)
				throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));

			m_Mutex->unlock_upgrade();
			m_Owns = false;
		}

		void swap(upgrade_lock& other) noexcept
		{
			std::swap(m_Mutex, other.m_Mutex);
			std::swap(m_Owns, other.m_Owns);
		}
		mutex_type* release() noexcept
		{
			mutex_type* result = m_Mutex;
			m_Mutex = nullptr;
			m_Owns = false;
			return result;
		}

		[[nodiscard]] mutex_type* mutex() const noexcept { return m_Mutex; }
		[[nodiscard]] bool owns_lock() const noexcept { return m_Owns; }
		explicit operator bool() const noexcept { return m_Owns; }

	private:
		void check_lockable() const
		{
			if (!m_Mutex)
				throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));
			if (m_Owns)
				throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur));
		}

		mutex_type* m_Mutex = nullptr;
		bool m_Owns = false;
	};

	/**
	 * Scoped upgrade in the style of boost::upgrade_to_unique_lock: the
	 * constructor takes an owning upgrade_lock and converts it to exclusive
	 * ownership via unlock_upgrade_and_lock(); the destructor downgrades via
	 * unlock_and_lock_upgrade() and returns upgrade ownership to the referenced
	 * upgrade_lock. While this object owns the exclusive lock, the source
	 * upgrade_lock reports owns_lock() == false.
	 */
	template<typename Mutex>
	class upgrade_to_unique_lock
	{
	public:
		using mutex_type = Mutex;

		explicit upgrade_to_unique_lock(upgrade_lock<Mutex>& lock) : m_Source(&lock)
		{
			if (!m_Source->owns_lock())
				throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));

			m_Source->mutex()->unlock_upgrade_and_lock();
			m_Mutex = m_Source->release();
		}

		~upgrade_to_unique_lock()
		{
			if (m_Mutex)
			{
				m_Mutex->unlock_and_lock_upgrade();
				*m_Source = upgrade_lock<Mutex>(*m_Mutex, std::adopt_lock);
			}
		}

		upgrade_to_unique_lock(const upgrade_to_unique_lock&) = delete;
		upgrade_to_unique_lock& operator=(const upgrade_to_unique_lock&) = delete;

		upgrade_to_unique_lock(upgrade_to_unique_lock&& other) noexcept :
			m_Source(other.m_Source), m_Mutex(other.m_Mutex)
		{
			other.m_Source = nullptr;
			other.m_Mutex = nullptr;
		}
		upgrade_to_unique_lock& operator=(upgrade_to_unique_lock&& other) noexcept
		{
			if (m_Mutex)
			{
				m_Mutex->unlock_and_lock_upgrade();
				*m_Source = upgrade_lock<Mutex>(*m_Mutex, std::adopt_lock);
			}

			m_Source = other.m_Source;
			m_Mutex = other.m_Mutex;
			other.m_Source = nullptr;
			other.m_Mutex = nullptr;
			return *this;
		}

		[[nodiscard]] bool owns_lock() const noexcept { return m_Mutex != nullptr; }
		explicit operator bool() const noexcept { return owns_lock(); }

	private:
		upgrade_lock<Mutex>* m_Source = nullptr;
		mutex_type* m_Mutex = nullptr;
	};
}

#ifndef MH_COMPILE_LIBRARY
#include "upgrade_mutex.inl"
#endif
