#include "mh/chrono/chrono_helpers.hpp"
#include <catch2/catch_all.hpp>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>

namespace
{
	// 2000-01-02 03:04:05 UTC
	constexpr std::time_t KNOWN_EPOCH = 946782245;
	// 2000-07-01 12:00:00 UTC (northern-hemisphere DST in most zones that use it)
	constexpr std::time_t KNOWN_EPOCH_SUMMER = 962452800;

	// Forces a specific (non-UTC) time zone for the duration of a test so that
	// UTC conversions which accidentally use local time produce a visibly wrong
	// result, and local-time conversions get exercised against DST rules,
	// independent of the machine's real configuration.
	class scoped_timezone
	{
	public:
		explicit scoped_timezone(const char* tz)
		{
			if (const char* old = std::getenv("TZ"))
				m_OldValue.emplace(old);

			set("TZ", tz);
		}
		~scoped_timezone()
		{
			set("TZ", m_OldValue ? m_OldValue->c_str() : nullptr);
		}

		scoped_timezone(const scoped_timezone&) = delete;
		scoped_timezone& operator=(const scoped_timezone&) = delete;

	private:
		static void set(const char* name, const char* value)
		{
#ifdef _WIN32
			_putenv_s(name, value ? value : "");
			_tzset();
#else
			if (value)
				setenv(name, value, 1);
			else
				unsetenv(name);
			tzset();
#endif
		}

		std::optional<std::string> m_OldValue;
	};
}

TEST_CASE("chrono_helpers - to_tm utc yields calendar fields independent of the local zone", "[chrono]")
{
	const scoped_timezone tz("America/New_York");

	const std::tm tm = mh::chrono::to_tm(KNOWN_EPOCH, mh::chrono::time_zone::utc);
	CHECK(tm.tm_year == 100); // years since 1900
	CHECK(tm.tm_mon == 0);    // january
	CHECK(tm.tm_mday == 2);
	CHECK(tm.tm_hour == 3);
	CHECK(tm.tm_min == 4);
	CHECK(tm.tm_sec == 5);
	CHECK(tm.tm_wday == 0); // sunday
}

TEST_CASE("chrono_helpers - to_time_t honors the utc zone argument", "[chrono]")
{
	// with a non-UTC local zone, interpreting these fields as local time would
	// be hours off; the utc argument must be respected
	const scoped_timezone tz("America/New_York");

	std::tm tm{};
	tm.tm_year = 100;
	tm.tm_mon = 0;
	tm.tm_mday = 2;
	tm.tm_hour = 3;
	tm.tm_min = 4;
	tm.tm_sec = 5;
	tm.tm_isdst = 0;

	REQUIRE(mh::chrono::to_time_t(tm, mh::chrono::time_zone::utc) == KNOWN_EPOCH);
}

TEST_CASE("chrono_helpers - utc round-trips are exact", "[chrono]")
{
	const scoped_timezone tz("America/New_York");

	for (const std::time_t timestamp : { KNOWN_EPOCH, KNOWN_EPOCH_SUMMER })
	{
		const std::tm tm = mh::chrono::to_tm(timestamp, mh::chrono::time_zone::utc);
		REQUIRE(mh::chrono::to_time_t(tm, mh::chrono::time_zone::utc) == timestamp);
	}
}

TEST_CASE("chrono_helpers - local round-trips are exact", "[chrono]")
{
	// America/New_York observes DST, so this covers both standard and daylight time
	const scoped_timezone tz("America/New_York");

	for (const std::time_t timestamp : { KNOWN_EPOCH, KNOWN_EPOCH_SUMMER })
	{
		const std::tm tm = mh::chrono::to_tm(timestamp, mh::chrono::time_zone::local);
		REQUIRE(mh::chrono::to_time_t(tm, mh::chrono::time_zone::local) == timestamp);
	}
}

TEST_CASE("chrono_helpers - time_point conversions", "[chrono]")
{
	const auto timePoint = std::chrono::system_clock::from_time_t(KNOWN_EPOCH);

	REQUIRE(mh::chrono::to_time_t(timePoint) == KNOWN_EPOCH);
	REQUIRE(mh::chrono::to_time_point(KNOWN_EPOCH) == timePoint);

	const std::tm tm = mh::chrono::to_tm(timePoint, mh::chrono::time_zone::utc);
	REQUIRE(mh::chrono::to_time_point(tm, mh::chrono::time_zone::utc) == timePoint);
}

TEST_CASE("chrono_helpers - current time accessors agree", "[chrono]")
{
	const std::time_t before = mh::chrono::current_time_t();
	const std::time_t viaTimePoint = mh::chrono::to_time_t(mh::chrono::current_time_point());
	const std::time_t after = mh::chrono::current_time_t();

	REQUIRE(before <= viaTimePoint);
	REQUIRE(viaTimePoint <= after);

	// smoke test: current_tm produces a plausible year (>= 2024, the test's era)
	const std::tm now = mh::chrono::current_tm(mh::chrono::time_zone::utc);
	REQUIRE(now.tm_year >= 124);
}

TEST_CASE("chrono_helpers - to_seconds", "[chrono]")
{
	using namespace std::chrono_literals;
	REQUIRE(mh::chrono::to_seconds(1500ms) == Catch::Approx(1.5));
	REQUIRE(mh::chrono::to_seconds<int>(2500ms) == 2);
}
