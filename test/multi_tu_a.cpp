// First translation unit of the multi-TU link test. See multi_tu_shared.hpp for
// what this pair of files is protecting against.
//
// regression: in header-only mode the dispatcher's thread-local static member
// definition was missing its inline marker, so ANY program with two TUs that
// include dispatcher.hpp failed to link (multiple definition). Merely linking
// this executable is the core of the test; the assertions below additionally
// prove the header entities are one program-wide instance each.
#include "multi_tu_shared.hpp"

#include <catch2/catch_all.hpp>

const std::thread::id* main_thread_id_address_tu_a()
{
	return &mh::main_thread_id;
}

bool is_main_thread_tu_a()
{
	return mh::is_main_thread();
}

#ifdef MH_COROUTINES_SUPPORTED
mh::dispatcher* try_get_dispatcher_tu_a()
{
	return mh::dispatcher::try_get();
}
#endif

TEST_CASE("multi-TU - header entities are program-wide, not per-TU")
{
	// regression: main_thread_id was declared `inline static`, which forces
	// internal linkage - every TU got its own copy (and is_main_thread() an ODR
	// violation). Catch2 runs this on the main thread, so both TUs must agree.
	REQUIRE(main_thread_id_address_tu_a() == main_thread_id_address_tu_b());
	REQUIRE(is_main_thread_tu_a());
	REQUIRE(is_main_thread_tu_b());

	// thread_sentinel.hpp must be usable from a TU that never defined MH_STUFF_API
	REQUIRE_NOTHROW(check_thread_sentinel_tu_b());
}

#ifdef MH_COROUTINES_SUPPORTED

TEST_CASE("multi-TU - dispatcher registration is visible across TUs")
{
	REQUIRE(try_get_dispatcher_tu_a() == nullptr);
	REQUIRE(try_get_dispatcher_tu_b() == nullptr);

	{
		mh::dispatcher d;
		d.register_for_current_thread();

		// One thread_local per thread - not one per TU
		REQUIRE(try_get_dispatcher_tu_a() == &d);
		REQUIRE(try_get_dispatcher_tu_b() == &d);
	}

	REQUIRE(try_get_dispatcher_tu_a() == nullptr);
	REQUIRE(try_get_dispatcher_tu_b() == nullptr);
}

TEST_CASE("multi-TU - coroutine machinery instantiated in another TU works")
{
	REQUIRE(ready_task_value_tu_b(21) == 21);
	REQUIRE(generator_sum_tu_b(4) == 6);
}

#endif
