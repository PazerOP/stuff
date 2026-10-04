#pragma once

#include <thread>

namespace mh
{
	inline const std::thread::id main_thread_id = std::this_thread::get_id();

	inline bool is_main_thread()
	{
		return main_thread_id == std::this_thread::get_id();
	}
}
