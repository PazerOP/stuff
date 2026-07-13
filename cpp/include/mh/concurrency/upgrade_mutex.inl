#ifdef MH_COMPILE_LIBRARY
#include "upgrade_mutex.hpp"
#endif

#ifndef MH_COMPILE_LIBRARY_INLINE
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

namespace mh
{
	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::lock()
	{
		std::unique_lock lock(m_Mutex);
		m_CV.wait(lock, [&]
			{
				return !m_HasExclusive && !m_HasUpgrade && !m_UpgradePending && m_SharedCount == 0;
			});
		m_HasExclusive = true;
	}

	MH_COMPILE_LIBRARY_INLINE bool upgrade_mutex::try_lock()
	{
		std::unique_lock lock(m_Mutex);
		if (m_HasExclusive || m_HasUpgrade || m_UpgradePending || m_SharedCount != 0)
			return false;

		m_HasExclusive = true;
		return true;
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::unlock()
	{
		{
			std::unique_lock lock(m_Mutex);
			m_HasExclusive = false;
		}
		m_CV.notify_all();
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::lock_shared()
	{
		std::unique_lock lock(m_Mutex);
		m_CV.wait(lock, [&]
			{
				return !m_HasExclusive && !m_UpgradePending;
			});
		m_SharedCount++;
	}

	MH_COMPILE_LIBRARY_INLINE bool upgrade_mutex::try_lock_shared()
	{
		std::unique_lock lock(m_Mutex);
		if (m_HasExclusive || m_UpgradePending)
			return false;

		m_SharedCount++;
		return true;
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::unlock_shared()
	{
		{
			std::unique_lock lock(m_Mutex);
			m_SharedCount--;
		}
		m_CV.notify_all();
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::lock_upgrade()
	{
		std::unique_lock lock(m_Mutex);
		m_CV.wait(lock, [&]
			{
				return !m_HasExclusive && !m_HasUpgrade && !m_UpgradePending;
			});
		m_HasUpgrade = true;
	}

	MH_COMPILE_LIBRARY_INLINE bool upgrade_mutex::try_lock_upgrade()
	{
		std::unique_lock lock(m_Mutex);
		if (m_HasExclusive || m_HasUpgrade || m_UpgradePending)
			return false;

		m_HasUpgrade = true;
		return true;
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::unlock_upgrade()
	{
		{
			std::unique_lock lock(m_Mutex);
			m_HasUpgrade = false;
		}
		m_CV.notify_all();
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::unlock_upgrade_and_lock()
	{
		std::unique_lock lock(m_Mutex);

		// Gate new shared/upgrade acquires first, then wait for existing shared
		// owners to drain. m_HasUpgrade stays true throughout, so no lock() caller
		// can take exclusive ownership between the drain and the conversion.
		m_UpgradePending = true;
		m_CV.wait(lock, [&]
			{
				return m_SharedCount == 0;
			});

		m_UpgradePending = false;
		m_HasUpgrade = false;
		m_HasExclusive = true;
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::unlock_and_lock_upgrade()
	{
		{
			std::unique_lock lock(m_Mutex);
			m_HasExclusive = false;
			m_HasUpgrade = true;
		}
		m_CV.notify_all();
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::unlock_and_lock_shared()
	{
		{
			std::unique_lock lock(m_Mutex);
			m_HasExclusive = false;
			m_SharedCount++;
		}
		m_CV.notify_all();
	}

	MH_COMPILE_LIBRARY_INLINE void upgrade_mutex::unlock_upgrade_and_lock_shared()
	{
		{
			std::unique_lock lock(m_Mutex);
			m_HasUpgrade = false;
			m_SharedCount++;
		}
		m_CV.notify_all();
	}
}
