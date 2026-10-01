#include "mh/concurrency/upgrade_mutex.hpp"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace
{
	// "must not yet have happened" settle window. Long enough that a broken
	// implementation reliably exposes itself, short enough to keep the suite fast.
	constexpr std::chrono::milliseconds SETTLE_TIME = 200ms;

	// "must eventually happen" deadline. Generous so the suite stays reliable
	// under ThreadSanitizer's 5-15x slowdown.
	constexpr std::chrono::seconds WAIT_DEADLINE = 10s;

	template<typename TFunc>
	bool eventually(TFunc&& func, std::chrono::steady_clock::duration timeout = WAIT_DEADLINE)
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (func())
				return true;

			std::this_thread::sleep_for(1ms);
		}

		return false;
	}

	// std::jthread is not available on all the libc++ versions CI builds with,
	// so roll a minimal join-on-destruction thread. Joining in the destructor
	// keeps a failed REQUIRE from tripping std::terminate via ~thread().
	class auto_join_thread
	{
	public:
		template<typename TFunc>
		explicit auto_join_thread(TFunc&& func) : m_Thread(std::forward<TFunc>(func)) {}
		~auto_join_thread() { join(); }

		auto_join_thread(const auto_join_thread&) = delete;
		auto_join_thread& operator=(const auto_join_thread&) = delete;

		void join()
		{
			if (m_Thread.joinable())
				m_Thread.join();
		}

	private:
		std::thread m_Thread;
	};

	// Runs a cleanup function on scope exit (including REQUIRE failure unwinds),
	// at most once even if also invoked explicitly.
	template<typename TFunc>
	class scope_guard
	{
	public:
		explicit scope_guard(TFunc func) : m_Func(std::move(func)) {}
		~scope_guard() { invoke(); }

		scope_guard(const scope_guard&) = delete;
		scope_guard& operator=(const scope_guard&) = delete;

		void invoke()
		{
			if (!m_Invoked)
			{
				m_Invoked = true;
				m_Func();
			}
		}

	private:
		TFunc m_Func;
		bool m_Invoked = false;
	};

	// Order-of-events log. Threads append; the main thread asserts on ordering
	// after joining (Catch2 assertions are not thread safe).
	class event_log
	{
	public:
		void add(std::string_view event)
		{
			std::lock_guard lock(m_Mutex);
			m_Events.emplace_back(event);
		}

		ptrdiff_t index_of(std::string_view event) const
		{
			std::lock_guard lock(m_Mutex);
			for (size_t i = 0; i < m_Events.size(); i++)
			{
				if (m_Events[i] == event)
					return static_cast<ptrdiff_t>(i);
			}

			return -1;
		}

		std::string str() const
		{
			std::lock_guard lock(m_Mutex);
			std::string result;
			for (const std::string& event : m_Events)
			{
				if (!result.empty())
					result += ", ";

				result += event;
			}

			return result;
		}

	private:
		mutable std::mutex m_Mutex;
		std::vector<std::string> m_Events;
	};

	// try_lock_shared probe that never retains ownership. Fails only while
	// exclusive is held or an upgrade transition is pending, so it doubles as a
	// "transition pending" detector when exclusive is known not to be held.
	bool probe_try_lock_shared(mh::upgrade_mutex& mutex)
	{
		if (mutex.try_lock_shared())
		{
			mutex.unlock_shared();
			return true;
		}

		return false;
	}

	// Verifies every ownership mode is fully released: try_lock only succeeds
	// with no shared owners, no upgrade owner, no exclusive owner, and no
	// pending transition.
	bool fully_released(mh::upgrade_mutex& mutex)
	{
		if (mutex.try_lock())
		{
			mutex.unlock();
			return true;
		}

		return false;
	}
}

TEST_CASE("upgrade_mutex - single-thread state machine")
{
	mh::upgrade_mutex m;

	// exclusive: lock/unlock, then re-acquire via try_lock
	m.lock();
	m.unlock();
	REQUIRE(fully_released(m));

	// shared: multiple times from one thread (distinct acquisitions, no recursion
	// involved: each lock_shared is a separate shared owner slot)
	m.lock_shared();
	m.lock_shared();
	REQUIRE(m.try_lock_shared());
	m.unlock_shared();
	m.unlock_shared();
	m.unlock_shared();
	REQUIRE(fully_released(m));

	// upgrade: acquire/release, then re-acquire via try_lock_upgrade
	m.lock_upgrade();
	m.unlock_upgrade();
	REQUIRE(m.try_lock_upgrade());
	m.unlock_upgrade();
	REQUIRE(fully_released(m));

	// upgrade -> exclusive with zero readers must complete without blocking
	// (single-threaded: anything else is a deadlock)
	m.lock_upgrade();
	m.unlock_upgrade_and_lock();
	m.unlock();
	REQUIRE(fully_released(m));

	// downgrade: exclusive -> upgrade
	m.lock();
	m.unlock_and_lock_upgrade();
	m.unlock_upgrade();
	REQUIRE(fully_released(m));

	// downgrade: exclusive -> shared
	m.lock();
	m.unlock_and_lock_shared();
	m.unlock_shared();
	REQUIRE(fully_released(m));

	// downgrade: upgrade -> shared
	m.lock_upgrade();
	m.unlock_upgrade_and_lock_shared();
	m.unlock_shared();
	REQUIRE(fully_released(m));
}

TEST_CASE("upgrade_mutex - try_lock matrix")
{
	mh::upgrade_mutex m;

	SECTION("try_lock fails while shared is held")
	{
		m.lock_shared();
		REQUIRE(!m.try_lock());
		m.unlock_shared();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock fails while upgrade is held")
	{
		m.lock_upgrade();
		REQUIRE(!m.try_lock());
		m.unlock_upgrade();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock fails while exclusive is held")
	{
		m.lock();
		REQUIRE(!m.try_lock());
		m.unlock();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock_shared fails while exclusive is held")
	{
		m.lock();
		REQUIRE(!m.try_lock_shared());
		m.unlock();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock_shared succeeds while upgrade is held with no transition")
	{
		m.lock_upgrade();
		REQUIRE(m.try_lock_shared());
		m.unlock_shared();
		m.unlock_upgrade();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock_upgrade fails while upgrade is held")
	{
		m.lock_upgrade();
		REQUIRE(!m.try_lock_upgrade());
		m.unlock_upgrade();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock_upgrade fails while exclusive is held")
	{
		m.lock();
		REQUIRE(!m.try_lock_upgrade());
		m.unlock();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock_upgrade succeeds while shared is held")
	{
		m.lock_shared();
		REQUIRE(m.try_lock_upgrade());
		m.unlock_upgrade();
		m.unlock_shared();
		REQUIRE(fully_released(m));
	}

	SECTION("try_lock_shared and try_lock_upgrade fail while a transition is pending")
	{
		std::atomic<bool> upgradeAcquired = false;

		m.lock_shared();
		auto_join_thread upgrader([&]
			{
				m.lock_upgrade();
				upgradeAcquired = true;
				m.unlock_upgrade_and_lock(); // blocks until the shared owner releases
				m.unlock();
			});
		scope_guard releaseShared([&] { m.unlock_shared(); });

		REQUIRE(eventually([&] { return upgradeAcquired.load(); }));

		// The upgrader has upgrade ownership; wait until its transition is
		// pending (observable as try_lock_shared failing while no exclusive
		// owner can exist yet - we still hold shared).
		REQUIRE(eventually([&] { return !probe_try_lock_shared(m); }));

		REQUIRE(!m.try_lock_shared());
		REQUIRE(!m.try_lock_upgrade());

		releaseShared.invoke(); // upgrader completes + releases
		upgrader.join();
		REQUIRE(fully_released(m));
	}
}

TEST_CASE("upgrade_mutex - pending upgrade gates out new shared acquires")
{
	// THE core semantic: while unlock_upgrade_and_lock() waits for existing
	// shared owners to drain, a NEW shared-lock attempt must not succeed until
	// the upgrader has taken (and released) its exclusive hold.
	mh::upgrade_mutex m;
	event_log log;

	std::atomic<bool> upgradeAcquired = false;
	std::atomic<bool> newReaderAcquired = false;

	// R (this thread): existing shared owner
	m.lock_shared();

	// U: acquires upgrade, then upgrades; the transition must block on R
	auto_join_thread upgrader([&]
		{
			m.lock_upgrade();
			upgradeAcquired = true;
			m.unlock_upgrade_and_lock();
			log.add("U:exclusive-acquired");
			// Hold exclusive briefly so a gate-jumping reader would overlap it
			std::this_thread::sleep_for(50ms);
			log.add("U:exclusive-releasing");
			m.unlock();
		});
	scope_guard releaseShared([&] { m.unlock_shared(); });

	REQUIRE(eventually([&] { return upgradeAcquired.load(); }));
	REQUIRE(eventually([&] { return !probe_try_lock_shared(m); })); // transition pending

	// N: new reader arriving while the transition is pending
	auto_join_thread newReader([&]
		{
			m.lock_shared();
			log.add("N:shared-acquired");
			newReaderAcquired = true;
			m.unlock_shared();
		});

	// While the upgrader is draining: new shared attempts must not succeed
	REQUIRE(!m.try_lock_shared());
	std::this_thread::sleep_for(SETTLE_TIME);
	REQUIRE(!newReaderAcquired);
	REQUIRE(!m.try_lock_shared());

	// Release the pre-existing reader; the upgrader must convert to exclusive
	// BEFORE the new reader gets shared ownership
	log.add("R:releasing");
	releaseShared.invoke();

	REQUIRE(eventually([&] { return newReaderAcquired.load(); }));
	upgrader.join();
	newReader.join();

	INFO("event order: " << log.str());
	REQUIRE(log.index_of("R:releasing") < log.index_of("U:exclusive-acquired"));
	REQUIRE(log.index_of("U:exclusive-acquired") < log.index_of("N:shared-acquired"));
	REQUIRE(log.index_of("U:exclusive-releasing") < log.index_of("N:shared-acquired"));
	REQUIRE(fully_released(m));
}

TEST_CASE("upgrade_mutex - only one thread can hold upgrade ownership")
{
	mh::upgrade_mutex m;

	std::atomic<bool> u2Acquired = false;
	std::atomic<bool> u2Upgraded = false;

	// U1 (this thread) holds upgrade: no second upgrade owner
	m.lock_upgrade();
	REQUIRE(!m.try_lock_upgrade());

	auto_join_thread u2([&]
		{
			m.lock_upgrade(); // blocks until U1 is completely done
			u2Acquired = true;
			m.unlock_upgrade_and_lock(); // U2 can itself upgrade
			u2Upgraded = true;
			m.unlock();
		});

	std::this_thread::sleep_for(SETTLE_TIME);
	REQUIRE(!u2Acquired);

	// U1 upgrades (no readers: immediate) - U2 must still be excluded
	m.unlock_upgrade_and_lock();
	REQUIRE(!u2Acquired);

	m.unlock();
	REQUIRE(eventually([&] { return u2Acquired.load(); }));
	REQUIRE(eventually([&] { return u2Upgraded.load(); }));
	u2.join();
	REQUIRE(fully_released(m));
}

TEST_CASE("upgrade_mutex - upgraders serialize their write sections")
{
	// N upgrader threads x M iterations of lock_upgrade -> upgrade -> mutate a
	// plain int -> unlock. Upgrade exclusivity + transition exclusivity make the
	// read-modify-write sections mutually exclusive, so the final count is
	// exact. A widened race window (yield between read and write) makes lost
	// updates near-certain if two threads ever hold the write side at once.
	constexpr int THREADS = 4;
	constexpr int ITERATIONS = 25;

	mh::upgrade_mutex m;
	int counter = 0;

	{
		std::vector<std::unique_ptr<auto_join_thread>> threads;
		for (int t = 0; t < THREADS; t++)
		{
			threads.push_back(std::make_unique<auto_join_thread>([&]
				{
					for (int i = 0; i < ITERATIONS; i++)
					{
						m.lock_upgrade();
						m.unlock_upgrade_and_lock();

						const int value = counter;
						std::this_thread::yield();
						counter = value + 1;

						m.unlock();
					}
				}));
		}
	}

	REQUIRE(counter == THREADS * ITERATIONS);
	REQUIRE(fully_released(m));
}

TEST_CASE("upgrade_mutex - downgrades")
{
	mh::upgrade_mutex m;

	const auto tryLockSharedFromOtherThread = [&]
	{
		bool acquired = false;
		auto_join_thread thread([&]
			{
				acquired = m.try_lock_shared();
				if (acquired)
					m.unlock_shared();
			});
		thread.join();
		return acquired;
	};
	const auto tryLockUpgradeFromOtherThread = [&]
	{
		bool acquired = false;
		auto_join_thread thread([&]
			{
				acquired = m.try_lock_upgrade();
				if (acquired)
					m.unlock_upgrade();
			});
		thread.join();
		return acquired;
	};

	SECTION("upgrade -> shared frees the upgrade slot")
	{
		m.lock_upgrade();
		REQUIRE(!tryLockUpgradeFromOtherThread());

		m.unlock_upgrade_and_lock_shared(); // must not block

		REQUIRE(tryLockUpgradeFromOtherThread()); // slot free while we hold shared
		REQUIRE(tryLockSharedFromOtherThread());
		REQUIRE(!m.try_lock()); // still a shared owner

		m.unlock_shared();
		REQUIRE(fully_released(m));
	}

	SECTION("exclusive -> shared lets readers join")
	{
		m.lock();
		REQUIRE(!tryLockSharedFromOtherThread());

		m.unlock_and_lock_shared(); // must not block

		REQUIRE(tryLockSharedFromOtherThread());
		REQUIRE(!m.try_lock());

		m.unlock_shared();
		REQUIRE(fully_released(m));
	}

	SECTION("exclusive -> upgrade lets readers join but not a second upgrader")
	{
		m.lock();
		REQUIRE(!tryLockSharedFromOtherThread());

		m.unlock_and_lock_upgrade(); // must not block

		REQUIRE(tryLockSharedFromOtherThread());
		REQUIRE(!tryLockUpgradeFromOtherThread());

		m.unlock_upgrade();
		REQUIRE(fully_released(m));
	}
}

TEST_CASE("upgrade_mutex - standard RAII wrappers")
{
	mh::upgrade_mutex m;

	SECTION("std::shared_lock")
	{
		{
			std::shared_lock lock(m);
			REQUIRE(lock.owns_lock());
			REQUIRE(!m.try_lock());
		}
		REQUIRE(fully_released(m));
	}

	SECTION("std::unique_lock")
	{
		{
			std::unique_lock lock(m);
			REQUIRE(lock.owns_lock());
			REQUIRE(!m.try_lock_shared());
		}
		REQUIRE(fully_released(m));
	}
}

TEST_CASE("upgrade_mutex - upgrade_lock")
{
	mh::upgrade_mutex m;

	SECTION("locking constructor")
	{
		{
			mh::upgrade_lock lock(m);
			REQUIRE(lock.owns_lock());
			REQUIRE(static_cast<bool>(lock));
			REQUIRE(lock.mutex() == &m);
			REQUIRE(!m.try_lock_upgrade());
			REQUIRE(m.try_lock_shared()); // shared still allowed
			m.unlock_shared();
		}
		REQUIRE(fully_released(m));
	}

	SECTION("defer_lock constructor + lock()")
	{
		mh::upgrade_lock lock(m, std::defer_lock);
		REQUIRE(!lock.owns_lock());
		REQUIRE(fully_released(m));

		lock.lock();
		REQUIRE(lock.owns_lock());
		REQUIRE(!m.try_lock_upgrade());

		lock.unlock();
		REQUIRE(!lock.owns_lock());
		REQUIRE(fully_released(m));
	}

	SECTION("try_to_lock constructor")
	{
		{
			mh::upgrade_lock lock(m, std::try_to_lock);
			REQUIRE(lock.owns_lock());
		}

		m.lock_upgrade();
		{
			mh::upgrade_lock lock(m, std::try_to_lock);
			REQUIRE(!lock.owns_lock());
		}
		m.unlock_upgrade();
		REQUIRE(fully_released(m));
	}

	SECTION("adopt_lock constructor")
	{
		m.lock_upgrade();
		{
			mh::upgrade_lock lock(m, std::adopt_lock);
			REQUIRE(lock.owns_lock());
		}
		REQUIRE(fully_released(m)); // destructor released the adopted ownership
	}

	SECTION("deferred try_lock")
	{
		mh::upgrade_lock lock(m, std::defer_lock);
		REQUIRE(lock.try_lock());
		REQUIRE(lock.owns_lock());
		REQUIRE(!m.try_lock_upgrade());
	}

	SECTION("move transfers ownership")
	{
		mh::upgrade_lock first(m);
		mh::upgrade_lock second(std::move(first));
		REQUIRE(!first.owns_lock());
		REQUIRE(first.mutex() == nullptr);
		REQUIRE(second.owns_lock());
		REQUIRE(second.mutex() == &m);

		mh::upgrade_lock<mh::upgrade_mutex> third;
		third = std::move(second);
		REQUIRE(!second.owns_lock());
		REQUIRE(third.owns_lock());

		third.unlock();
		REQUIRE(fully_released(m));
	}

	SECTION("release() abandons ownership without unlocking")
	{
		mh::upgrade_lock lock(m);
		REQUIRE(lock.release() == &m);
		REQUIRE(!lock.owns_lock());
		REQUIRE(!m.try_lock_upgrade()); // still locked
		m.unlock_upgrade();
		REQUIRE(fully_released(m));
	}

	SECTION("error handling matches std::shared_lock")
	{
		mh::upgrade_lock<mh::upgrade_mutex> unattached;
		REQUIRE_THROWS_AS(unattached.lock(), std::system_error);
		REQUIRE_THROWS_AS(unattached.unlock(), std::system_error);

		mh::upgrade_lock owned(m);
		REQUIRE_THROWS_AS(owned.lock(), std::system_error);
	}
}

TEST_CASE("upgrade_mutex - upgrade_to_unique_lock round trip")
{
	mh::upgrade_mutex m;

	mh::upgrade_lock upgradeLock(m);
	REQUIRE(upgradeLock.owns_lock());

	{
		mh::upgrade_to_unique_lock exclusiveLock(upgradeLock);
		REQUIRE(exclusiveLock.owns_lock());
		REQUIRE(static_cast<bool>(exclusiveLock));

		// exclusive ownership: no readers; source lock gave up its ownership
		REQUIRE(!upgradeLock.owns_lock());
		REQUIRE(!m.try_lock_shared());
	}

	// destroyed: upgrade ownership restored to the source lock
	REQUIRE(upgradeLock.owns_lock());
	REQUIRE(m.try_lock_shared()); // shared allowed alongside upgrade again
	m.unlock_shared();
	REQUIRE(!m.try_lock_upgrade()); // the slot is still owned by upgradeLock

	upgradeLock.unlock();
	REQUIRE(fully_released(m));

	// constructing from a non-owning upgrade_lock throws
	mh::upgrade_lock deferred(m, std::defer_lock);
	REQUIRE_THROWS_AS(mh::upgrade_to_unique_lock<mh::upgrade_mutex>(deferred), std::system_error);
	REQUIRE(fully_released(m));
}

TEST_CASE("upgrade_mutex - stress")
{
	// Readers verify a multi-word invariant (two counters that must always
	// match); every mutation goes through exclusive ownership with a widened
	// torn-state window in between. Any failure of exclusion shows up as a
	// violation count (and as a data race under ThreadSanitizer).
	constexpr int READERS = 4;
	constexpr int UPGRADERS = 2;
	constexpr int WRITERS = 2;
	constexpr std::chrono::seconds STRESS_TIME = 2s;

	mh::upgrade_mutex m;

	struct
	{
		size_t a = 0;
		size_t b = 0;
	} data;

	std::atomic<bool> stop = false;
	std::atomic<size_t> violations = 0;
	std::atomic<size_t> totalIncrements = 0;

	const auto mutate = [&]
	{
		// deliberately torn: a != b is visible to any concurrent reader
		data.a++;
		std::this_thread::yield();
		data.b++;
	};
	const auto checkInvariant = [&]
	{
		if (data.a != data.b)
			violations++;
	};

	{
		std::vector<std::unique_ptr<auto_join_thread>> threads;
		scope_guard stopGuard([&] { stop = true; }); // even if thread creation throws

		for (int t = 0; t < READERS; t++)
		{
			threads.push_back(std::make_unique<auto_join_thread>([&]
				{
					while (!stop)
					{
						m.lock_shared();
						checkInvariant();
						m.unlock_shared();
					}
				}));
		}

		for (int t = 0; t < UPGRADERS; t++)
		{
			threads.push_back(std::make_unique<auto_join_thread>([&]
				{
					size_t increments = 0;
					for (size_t i = 0; !stop; i++)
					{
						m.lock_upgrade();

						if (i % 4 == 3)
						{
							// downgrade without writing; recheck under shared
							m.unlock_upgrade_and_lock_shared();
							checkInvariant();
							m.unlock_shared();
							continue;
						}

						m.unlock_upgrade_and_lock();
						mutate();
						increments++;

						switch (i % 3)
						{
						case 0:
							m.unlock();
							break;
						case 1:
							m.unlock_and_lock_shared();
							checkInvariant();
							m.unlock_shared();
							break;
						default:
							m.unlock_and_lock_upgrade();
							m.unlock_upgrade();
							break;
						}
					}

					totalIncrements += increments;
				}));
		}

		for (int t = 0; t < WRITERS; t++)
		{
			threads.push_back(std::make_unique<auto_join_thread>([&]
				{
					size_t increments = 0;
					while (!stop)
					{
						m.lock();
						mutate();
						increments++;
						m.unlock();
					}

					totalIncrements += increments;
				}));
		}

		std::this_thread::sleep_for(STRESS_TIME);
		stop = true;
	} // all threads joined here

	REQUIRE(violations == 0);
	REQUIRE(data.a == data.b);
	REQUIRE(data.a == totalIncrements);
	REQUIRE(totalIncrements > 0);
	REQUIRE(fully_released(m));
}
