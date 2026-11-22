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
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include "last_include.hpp"

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
	// child. This also covers the monitor coroutine was also a capturing lambda
	// whose closure died before the first resume.
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
	// regression. A child that exited before the SIGCHLD handler was installed
	// (or before registration) had its signal dropped. Nothing ever woke the
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

TEST_CASE("process - a failed spawn is reported to the parent and leaves no child behind", "[process]")
{
	// posix_spawn reports a failed exec synchronously, so start() throws in the
	// parent. No forked child ever runs a copy of the parent's code (which
	// would unwind its stack and run its atexit handlers).
	mh::dispatcher disp;
	disp.register_for_current_thread();

	mh::process p("/nonexistent-binary-mh-stuff-test", {});
	CHECK_THROWS_AS(p.start(), std::runtime_error);
	CHECK_FALSE(p.is_running());

	// Nothing was spawned, so there is no child to reap. wait4 does exactly what
	// waitpid does here; waitpid itself is poisoned by last_include.hpp.
	int status = 0;
	errno = 0;
	CHECK(wait4(-1, &status, WNOHANG, nullptr) == -1);
	CHECK(errno == ECHILD);

	auto t = p.wait_async();
	REQUIRE(t.is_ready());
	CHECK(t.get() == -1);
}

TEST_CASE("process - io redirection is configured in the parent, before the child exists", "[process]")
{
	// Redirection goes through posix_spawn file actions here, so a sink that
	// cannot be redirected fails in the parent. No forked child is left to
	// unwind a copy of the parent's stack.
	auto sink = std::make_shared<mh::io::fd_sink>(STDOUT_FILENO, false);
	sink->close();
	mh::process p("/bin/cat", {}, nullptr, sink);
	CHECK_THROWS_AS(p.start(), std::runtime_error);

	int status = 0;
	errno = 0;
	const pid_t r = wait4(-1, &status, WNOHANG, nullptr);
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
