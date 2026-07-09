// Second translation unit of the multi-TU link test. See multi_tu_shared.hpp
// for what this pair of files is protecting against. No test cases live here -
// this TU only has to (a) compile, (b) link together with multi_tu_a.cpp, and
// (c) hand out its view of entities that must be program-wide.
#include "multi_tu_shared.hpp"

const std::thread::id* main_thread_id_address_tu_b()
{
	return &mh::main_thread_id;
}

bool is_main_thread_tu_b()
{
	return mh::is_main_thread();
}

void check_thread_sentinel_tu_b()
{
	mh::thread_sentinel sentinel;
	// Pass the location explicitly: on toolchains without std::source_location
	// (e.g. clang-14/libc++-14), MH_SOURCE_LOCATION_AUTO degrades to a
	// parameter with no default argument.
	sentinel.check(MH_SOURCE_LOCATION_CURRENT());
}

#ifdef MH_COROUTINES_SUPPORTED

mh::dispatcher* try_get_dispatcher_tu_b()
{
	return mh::dispatcher::try_get();
}

int ready_task_value_tu_b(int value)
{
	return mh::make_ready_task<int>(value).get();
}

int generator_sum_tu_b(int count)
{
	auto range = [](int n) -> mh::generator<int>
	{
		for (int i = 0; i < n; i++)
			co_yield i;
	};

	int sum = 0;
	for (int v : range(count))
		sum += v;

	return sum;
}

#endif
