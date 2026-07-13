#include "mh/process/process.hpp"

#ifdef __unix__

#include "mh/concurrency/dispatcher.hpp"
#include "mh/error/not_implemented_error.hpp"
#include "mh/io/fd_sink.hpp"

#include <catch2/catch_all.hpp>

#include <cerrno>
#include <chrono>
#include <memory>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std::chrono_literals;

namespace
{
	// Pump the current thread's dispatcher until the task completes (bounded so a
	// regression turns into a test failure, not a hung CI job).
	template<typename T>
	bool pump_until_ready(mh::dispatcher& disp, const mh::task<T>& t,
		std::chrono::steady_clock::duration timeout = 25s)
	{
		const auto end = std::chrono::steady_clock::now() + timeout;
		while (!t.is_ready() && std::chrono::steady_clock::now() < end)
		{
			disp.run_for(5ms);
			std::this_thread::sleep_for(1ms);
		}

		return t.is_ready();
	}
}

TEST_CASE("process - wait_async completes while the child is still running", "[process]")
{
	// regression: the process manager used to resume waiter coroutines while
	// holding its own (non-recursive) mutex, deadlocking on the first completed
	// child; the monitor coroutine was also a capturing lambda whose closure died
	// before the first resume.
	mh::dispatcher disp;
	disp.register_for_current_thread();

	mh::process p("/bin/sleep", {"0.2"});
	REQUIRE(p.start());
	REQUIRE(p.get_pid() > 0);

	auto t = p.wait_async();
	REQUIRE(pump_until_ready(disp, t));
	CHECK(t.get() == 0);
}

TEST_CASE("process - waiting on children that exit immediately never hangs", "[process]")
{
	// regression: a child that exited before the SIGCHLD handler was installed
	// (or before registration) had its signal dropped, so nothing ever woke the
	// monitor and wait_async suspended forever.
	mh::dispatcher disp;
	disp.register_for_current_thread();

	for (int i = 0; i < 25; i++)
	{
		mh::process p("/bin/true", {});
		REQUIRE(p.start());

		auto t = p.wait_async();
		REQUIRE(pump_until_ready(disp, t));
		CHECK(t.get() == 0);
	}
}

TEST_CASE("process - exit codes are reported", "[process]")
{
	mh::dispatcher disp;
	disp.register_for_current_thread();

	mh::process p("/bin/false", {});
	REQUIRE(p.start());

	auto t = p.wait_async();
	REQUIRE(pump_until_ready(disp, t));
	CHECK(t.get() == 1);
}

TEST_CASE("process - exec failure reports 127 without duplicating the parent", "[process]")
{
	// regression: the child used to exit(1) after a failed exec, running atexit
	// handlers and flushing stdio buffers duplicated from the parent. _exit(127)
	// follows the shell convention and keeps the child side-effect free.
	mh::dispatcher disp;
	disp.register_for_current_thread();

	mh::process p("/nonexistent-binary-mh-stuff-test", {});
	REQUIRE(p.start()); // fork succeeds; exec fails inside the child

	auto t = p.wait_async();
	REQUIRE(pump_until_ready(disp, t));
	CHECK(t.get() == 127);
}

TEST_CASE("process - unimplemented io redirection fails in the parent, pre-fork", "[process]")
{
	// regression: this used to throw inside the FORKED CHILD, unwinding a copy of
	// the parent's stack - a caller that catches would keep a duplicate of the
	// entire application running.
	auto sink = std::make_shared<mh::io::fd_sink>(STDOUT_FILENO, false);
	mh::process p("/bin/cat", {}, nullptr, sink);
	CHECK_THROWS_AS(p.start(), mh::not_implemented_error);

	// and no child process may be left behind
	int status;
	errno = 0;
	const pid_t r = waitpid(-1, &status, WNOHANG);
	CHECK(r == -1);
	CHECK(errno == ECHILD);
}

TEST_CASE("process - wait_async on a process that was never started", "[process]")
{
	mh::process p("/bin/true", {});
	auto t = p.wait_async(); // completes synchronously
	REQUIRE(t.is_ready());
	CHECK(t.get() == -1);
}

TEST_CASE("process - terminate ends a long-running child", "[process]")
{
	mh::dispatcher disp;
	disp.register_for_current_thread();

	mh::process p("/bin/sleep", {"30"});

	// terminating a process that was never started has nothing to signal
	CHECK_FALSE(p.terminate(false));
	CHECK_FALSE(p.terminate(true));

	REQUIRE(p.start());
	REQUIRE(p.is_running());
	CHECK_FALSE(p.start()); // start is one-shot

	auto t = p.wait_async();
	REQUIRE(p.terminate(true)); // SIGKILL
	REQUIRE(pump_until_ready(disp, t));
	CHECK(t.get() == -SIGKILL); // signaled exits report -signo
	CHECK_FALSE(p.is_running());

	// completed: terminate refuses too (the pid may already be reused)
	CHECK_FALSE(p.terminate(true));
}

#endif // __unix__
