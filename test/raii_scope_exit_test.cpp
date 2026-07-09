#include "mh/raii/scope_exit.hpp"
#include <catch2/catch_all.hpp>

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{
	// Copyable functor whose move constructor may throw: per the P0052 rules a
	// scope guard must COPY it when transferred, never move it.
	struct throwing_move_functor
	{
		inline static int s_Copies = 0;
		inline static int s_Moves = 0;
		inline static int s_Calls = 0;

		throwing_move_functor() = default;
		throwing_move_functor(const throwing_move_functor&) noexcept { ++s_Copies; }
		throwing_move_functor(throwing_move_functor&&) noexcept(false) { ++s_Moves; }
		void operator()() const { ++s_Calls; }

		static void reset_counters() { s_Copies = s_Moves = s_Calls = 0; }
	};

	// Control case: nothrow-movable functor, which the guard is allowed to move.
	struct nothrow_move_functor
	{
		inline static int s_Copies = 0;
		inline static int s_Moves = 0;
		inline static int s_Calls = 0;

		nothrow_move_functor() = default;
		nothrow_move_functor(const nothrow_move_functor&) noexcept { ++s_Copies; }
		nothrow_move_functor(nothrow_move_functor&&) noexcept { ++s_Moves; }
		void operator()() const { ++s_Calls; }

		static void reset_counters() { s_Copies = s_Moves = s_Calls = 0; }
	};

	bool g_UnwindSuccessRan = false;
	bool g_UnwindFailRan = false;

	// Destructor that runs scope guards while the stack is unwinding; the
	// guarded scope itself exits normally.
	struct guards_in_dtor_probe
	{
		~guards_in_dtor_probe()
		{
			mh::scope_success success([] { g_UnwindSuccessRan = true; });
			mh::scope_fail fail([] { g_UnwindFailRan = true; });
		}
	};
}

// The repo's CI promotes warnings to errors, so simply constructing these
// guards (which instantiates the converting constructors) is itself a test.
TEST_CASE("scope_exit - runs the exit function when the scope ends", "[raii][scope_exit]")
{
	int fired = 0;

	{
		mh::scope_exit guard([&] { ++fired; });
		REQUIRE(guard);
		REQUIRE(fired == 0);
	}
	REQUIRE(fired == 1);
}

TEST_CASE("scope_exit - release() disarms the guard", "[raii][scope_exit]")
{
	int fired = 0;

	{
		mh::scope_exit guard([&] { ++fired; });
		guard.release();
		REQUIRE(!guard);
	}
	REQUIRE(fired == 0);
}

TEST_CASE("scope_exit - can be constructed disabled", "[raii][scope_exit]")
{
	int fired = 0;

	{
		auto increment = [&] { ++fired; };
		mh::scope_exit<decltype(increment)> guard(increment, false);
		REQUIRE(!guard);
	}
	REQUIRE(fired == 0);
}

TEST_CASE("scope_exit - transferring a throwing-move functor copies it", "[raii][scope_exit]")
{
	static_assert(!std::is_nothrow_move_constructible_v<throwing_move_functor>);
	static_assert(std::is_nothrow_copy_constructible_v<throwing_move_functor>);

	throwing_move_functor::reset_counters();
	{
		throwing_move_functor functor;
		mh::scope_exit<throwing_move_functor> guard(functor);
		REQUIRE(throwing_move_functor::s_Copies == 1);

		// The guard's move constructor is declared noexcept, so it must never
		// run the (potentially throwing) move constructor of the functor.
		mh::scope_exit<throwing_move_functor> movedTo(std::move(guard));
		REQUIRE(throwing_move_functor::s_Moves == 0);
		REQUIRE(throwing_move_functor::s_Copies == 2);
		REQUIRE(!guard);
		REQUIRE(movedTo);
	}
	REQUIRE(throwing_move_functor::s_Calls == 1); // only the surviving guard fired
}

TEST_CASE("scope_exit - transferring a nothrow-move functor moves it", "[raii][scope_exit]")
{
	nothrow_move_functor::reset_counters();
	{
		nothrow_move_functor functor;
		mh::scope_exit<nothrow_move_functor> guard(functor);
		REQUIRE(nothrow_move_functor::s_Copies == 1);

		mh::scope_exit<nothrow_move_functor> movedTo(std::move(guard));
		REQUIRE(nothrow_move_functor::s_Moves == 1);
		REQUIRE(nothrow_move_functor::s_Copies == 1);
	}
	REQUIRE(nothrow_move_functor::s_Calls == 1);
}

TEST_CASE("scope_exit - constructor noexcept matches the construction actually performed", "[raii][scope_exit]")
{
	// large capture: constructing a std::function from this lambda allocates,
	// so the guard's converting constructor must NOT claim to be noexcept
	char payload[256] = {};
	auto lambda = [payload]() { (void)payload; };
	using exit_function = std::function<void()>;

	STATIC_REQUIRE(!std::is_nothrow_constructible_v<exit_function, decltype(lambda)&>);
	STATIC_REQUIRE(!noexcept(mh::scope_exit<exit_function>(lambda)));

	// ...and the guard still works
	bool ran = false;
	{
		auto setRan = [&ran, payload]() { (void)payload; ran = true; };
		mh::scope_exit<exit_function> guard(setRan);
	}
	REQUIRE(ran);
}

TEST_CASE("scope_fail/scope_success - normal flow", "[raii][scope_exit]")
{
	SECTION("no exception: success fires, fail does not")
	{
		bool successRan = false;
		bool failRan = false;
		{
			mh::scope_success success([&] { successRan = true; });
			mh::scope_fail fail([&] { failRan = true; });
		}
		REQUIRE(successRan);
		REQUIRE(!failRan);
	}
	SECTION("exception unwinds the scope: fail fires, success does not")
	{
		bool successRan = false;
		bool failRan = false;
		try
		{
			mh::scope_success success([&] { successRan = true; });
			mh::scope_fail fail([&] { failRan = true; });
			throw std::runtime_error("unwind");
		}
		catch (const std::runtime_error&)
		{
		}
		REQUIRE(!successRan);
		REQUIRE(failRan);
	}
}

TEST_CASE("scope_fail/scope_success - correct inside a destructor during unwinding", "[raii][scope_exit]")
{
	g_UnwindSuccessRan = false;
	g_UnwindFailRan = false;

	// guards_in_dtor_probe's destructor runs while an exception is in flight,
	// but its own scope completes normally: success must fire, fail must not
	try
	{
		guards_in_dtor_probe probe;
		throw std::runtime_error("unwind");
	}
	catch (const std::runtime_error&)
	{
	}

	REQUIRE(g_UnwindSuccessRan);
	REQUIRE(!g_UnwindFailRan);
}
