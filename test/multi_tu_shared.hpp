#pragma once

// Shared include set + cross-TU probes for the multi-translation-unit link test.

// thread_sentinel.hpp goes first: it must compile without MH_STUFF_API having
// been defined by an earlier include or the build system.
#include <mh/concurrency/thread_sentinel.hpp>

#include <mh/concurrency/async.hpp>
#include <mh/concurrency/dispatcher.hpp>
#include <mh/concurrency/main_thread.hpp>
#include <mh/concurrency/thread_pool.hpp>
#include <mh/coroutine/future.hpp>
#include <mh/coroutine/generator.hpp>
#include <mh/coroutine/task.hpp>
#include <mh/coroutine/thread.hpp>
#include <mh/future.hpp>
#include <mh/text/pooled_string.hpp>

#include <thread>

// Every probe below is defined in exactly one TU; comparing results across TUs
// proves the underlying entities are program-wide, not silently per-TU.

const std::thread::id* main_thread_id_address_tu_a();
const std::thread::id* main_thread_id_address_tu_b();

bool is_main_thread_tu_a();
bool is_main_thread_tu_b();

void check_thread_sentinel_tu_b();

mh::pooled_string pooled_string_tu_a(std::string_view str);
mh::pooled_string pooled_string_tu_b(std::string_view str);

#ifdef MH_COROUTINES_SUPPORTED
mh::dispatcher* try_get_dispatcher_tu_a();
mh::dispatcher* try_get_dispatcher_tu_b();

int ready_task_value_tu_b(int value);
int generator_sum_tu_b(int count);
#endif
