// Second translation unit of the multi-TU link test. See multi_tu_shared.hpp
// for what this pair of files is protecting against.
#include "multi_tu_shared.hpp"
#include "last_include.hpp"

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
	// Pass the location explicitly.
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
