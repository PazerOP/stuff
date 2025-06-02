#include "mh/chrono/chrono_helpers.hpp"
#include <catch2/catch_all.hpp>

#include <chrono>
#include <ctime>
#include <thread>

TEST_CASE("to_seconds conversion", "[chrono][chrono_helpers]")
{
	SECTION("milliseconds to double seconds")
	{
		auto duration = std::chrono::milliseconds(1500);
		auto seconds = mh::chrono::to_seconds(duration);
		REQUIRE(seconds == Catch::Approx(1.5));
	}

	SECTION("microseconds to double seconds")
	{
		auto duration = std::chrono::microseconds(1500000);
		auto seconds = mh::chrono::to_seconds(duration);
		REQUIRE(seconds == Catch::Approx(1.5));
	}

	SECTION("minutes to float seconds")
	{
		auto duration = std::chrono::minutes(2);
		auto seconds = mh::chrono::to_seconds<float>(duration);
		REQUIRE(seconds == Catch::Approx(120.0f));
	}

	SECTION("zero duration")
	{
		auto duration = std::chrono::seconds(0);
		auto seconds = mh::chrono::to_seconds(duration);
		REQUIRE(seconds == Catch::Approx(0.0));
	}
}

TEST_CASE("time_t conversions", "[chrono][chrono_helpers]")
{
	SECTION("time_point to time_t and back")
	{
		auto now = std::chrono::system_clock::now();
		auto time_t_val = mh::chrono::to_time_t(now);
		auto back_to_time_point = mh::chrono::to_time_point(time_t_val);
		
		// Should be within 1 second due to precision loss
		auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - back_to_time_point);
		REQUIRE(std::abs(diff.count()) <= 1);
	}

	SECTION("tm to time_t local timezone")
	{
		std::tm test_tm{};
		test_tm.tm_year = 123; // 2023
		test_tm.tm_mon = 0;    // January
		test_tm.tm_mday = 1;   // 1st
		test_tm.tm_hour = 12;
		test_tm.tm_min = 0;
		test_tm.tm_sec = 0;
		test_tm.tm_isdst = -1; // Let system determine DST

		auto time_t_val = mh::chrono::to_time_t(test_tm, mh::chrono::time_zone::local);
		REQUIRE(time_t_val != -1);
	}
}

TEST_CASE("tm conversions", "[chrono][chrono_helpers]")
{
	SECTION("time_point to tm local timezone")
	{
		auto now = std::chrono::system_clock::now();
		auto tm_val = mh::chrono::to_tm(now, mh::chrono::time_zone::local);
		
		// Basic sanity checks
		REQUIRE(tm_val.tm_year >= 123); // At least 2023
		REQUIRE(tm_val.tm_mon >= 0);
		REQUIRE(tm_val.tm_mon <= 11);
		REQUIRE(tm_val.tm_mday >= 1);
		REQUIRE(tm_val.tm_mday <= 31);
		REQUIRE(tm_val.tm_hour >= 0);
		REQUIRE(tm_val.tm_hour <= 23);
	}

	SECTION("time_point to tm UTC timezone")
	{
		auto now = std::chrono::system_clock::now();
		auto tm_val = mh::chrono::to_tm(now, mh::chrono::time_zone::utc);
		
		// Basic sanity checks
		REQUIRE(tm_val.tm_year >= 123); // At least 2023
		REQUIRE(tm_val.tm_mon >= 0);
		REQUIRE(tm_val.tm_mon <= 11);
		REQUIRE(tm_val.tm_mday >= 1);
		REQUIRE(tm_val.tm_mday <= 31);
		REQUIRE(tm_val.tm_hour >= 0);
		REQUIRE(tm_val.tm_hour <= 23);
	}

	SECTION("time_t to tm local timezone")
	{
		auto now_time_t = std::time(nullptr);
		auto tm_val = mh::chrono::to_tm(now_time_t, mh::chrono::time_zone::local);
		
		// Basic sanity checks
		REQUIRE(tm_val.tm_year >= 123); // At least 2023
		REQUIRE(tm_val.tm_mon >= 0);
		REQUIRE(tm_val.tm_mon <= 11);
	}

	SECTION("time_t to tm UTC timezone")
	{
		auto now_time_t = std::time(nullptr);
		auto tm_val = mh::chrono::to_tm(now_time_t, mh::chrono::time_zone::utc);
		
		// Basic sanity checks
		REQUIRE(tm_val.tm_year >= 123); // At least 2023
		REQUIRE(tm_val.tm_mon >= 0);
		REQUIRE(tm_val.tm_mon <= 11);
	}
}

TEST_CASE("tm to time_point conversions", "[chrono][chrono_helpers]")
{
	SECTION("tm to time_point local timezone")
	{
		std::tm test_tm{};
		test_tm.tm_year = 123; // 2023
		test_tm.tm_mon = 5;    // June
		test_tm.tm_mday = 15;  // 15th
		test_tm.tm_hour = 14;
		test_tm.tm_min = 30;
		test_tm.tm_sec = 45;
		test_tm.tm_isdst = -1; // Let system determine DST

		auto time_point = mh::chrono::to_time_point(test_tm, mh::chrono::time_zone::local);
		
		// Convert back to verify
		auto back_to_tm = mh::chrono::to_tm(time_point, mh::chrono::time_zone::local);
		REQUIRE(back_to_tm.tm_year == 123);
		REQUIRE(back_to_tm.tm_mon == 5);
		REQUIRE(back_to_tm.tm_mday == 15);
		REQUIRE(back_to_tm.tm_hour == 14);
		REQUIRE(back_to_tm.tm_min == 30);
		REQUIRE(back_to_tm.tm_sec == 45);
	}
}

TEST_CASE("current time functions", "[chrono][chrono_helpers]")
{
	SECTION("current_time_t")
	{
		auto time1 = mh::chrono::current_time_t();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		auto time2 = mh::chrono::current_time_t();
		
		REQUIRE(time2 >= time1);
		REQUIRE(time2 - time1 <= 2); // Should be within 2 seconds
	}

	SECTION("current_time_point")
	{
		auto tp1 = mh::chrono::current_time_point();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		auto tp2 = mh::chrono::current_time_point();
		
		REQUIRE(tp2 > tp1);
		auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(tp2 - tp1);
		REQUIRE(diff.count() >= 10);
		REQUIRE(diff.count() <= 1000); // Should be within 1 second
	}

	SECTION("current_tm local timezone")
	{
		auto tm_val = mh::chrono::current_tm(mh::chrono::time_zone::local);
		
		// Basic sanity checks
		REQUIRE(tm_val.tm_year >= 123); // At least 2023
		REQUIRE(tm_val.tm_mon >= 0);
		REQUIRE(tm_val.tm_mon <= 11);
		REQUIRE(tm_val.tm_mday >= 1);
		REQUIRE(tm_val.tm_mday <= 31);
		REQUIRE(tm_val.tm_hour >= 0);
		REQUIRE(tm_val.tm_hour <= 23);
		REQUIRE(tm_val.tm_min >= 0);
		REQUIRE(tm_val.tm_min <= 59);
		REQUIRE(tm_val.tm_sec >= 0);
		REQUIRE(tm_val.tm_sec <= 60); // 60 for leap seconds
	}

	SECTION("current_tm UTC timezone")
	{
		auto tm_val = mh::chrono::current_tm(mh::chrono::time_zone::utc);
		
		// Basic sanity checks
		REQUIRE(tm_val.tm_year >= 123); // At least 2023
		REQUIRE(tm_val.tm_mon >= 0);
		REQUIRE(tm_val.tm_mon <= 11);
		REQUIRE(tm_val.tm_mday >= 1);
		REQUIRE(tm_val.tm_mday <= 31);
		REQUIRE(tm_val.tm_hour >= 0);
		REQUIRE(tm_val.tm_hour <= 23);
		REQUIRE(tm_val.tm_min >= 0);
		REQUIRE(tm_val.tm_min <= 59);
		REQUIRE(tm_val.tm_sec >= 0);
		REQUIRE(tm_val.tm_sec <= 60); // 60 for leap seconds
	}
}

TEST_CASE("timezone consistency", "[chrono][chrono_helpers]")
{
	SECTION("local vs UTC time difference")
	{
		auto now = mh::chrono::current_time_point();
		auto local_tm = mh::chrono::to_tm(now, mh::chrono::time_zone::local);
		auto utc_tm = mh::chrono::to_tm(now, mh::chrono::time_zone::utc);
		
		// The hour difference should be reasonable (timezone offset)
		// This test might be fragile around DST changes, but should generally work
		auto hour_diff = std::abs(local_tm.tm_hour - utc_tm.tm_hour);
		REQUIRE(hour_diff <= 24); // Should be within 24 hours (accounting for date rollover)
	}
}