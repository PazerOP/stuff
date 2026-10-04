#include "mh/io/fd_sink.hpp"
#include "mh/io/fd_source.hpp"

#ifdef __unix__

#include <catch2/catch_all.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

using namespace std::chrono_literals;

namespace
{
	void noop_signal_handler(int) {}
}

TEST_CASE("fd_sink/fd_source - pipe round trip", "[io][fd]")
{
	// regression: in header-only mode these classes used to be declared but never
	// defined (their headers did not include the .inl), so ANY use failed to link.
	int fds[2];
	REQUIRE(pipe(fds) == 0);

	mh::io::fd_sink sink(fds[1], true);
	mh::io::fd_source source(fds[0], true);
	REQUIRE(sink.is_open());
	REQUIRE(source.is_open());

	auto wt = sink.write_async("hello", 5);
	REQUIRE(wt.get() == 5);

	char buf[16] = {};
	auto rt = source.read_async(buf, sizeof(buf));
	REQUIRE(rt.get() == 5);
	CHECK(std::memcmp(buf, "hello", 5) == 0);
}

TEST_CASE("fd_source - a signal interrupting a blocked read is retried, not an error", "[io][fd]")
{
	// regression: EINTR (e.g. from this library's own SIGCHLD handler) used to
	// surface as a thrown "Failed to read from file descriptor".
	struct sigaction sa {};
	sa.sa_handler = &noop_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0; // deliberately NOT SA_RESTART
	REQUIRE(sigaction(SIGUSR1, &sa, nullptr) == 0);

	int fds[2];
	REQUIRE(pipe(fds) == 0);
	mh::io::fd_source source(fds[0], true);

	size_t bytes_read = 0;
	char buf[16] = {};
	bool threw = false;
	std::thread reader([&]
	{
		try
		{
			// read_async runs eagerly: this blocks inside the call expression
			auto t = source.read_async(buf, sizeof(buf));
			bytes_read = t.get();
		}
		catch (...)
		{
			threw = true;
		}
	});

	std::this_thread::sleep_for(200ms);
	pthread_kill(reader.native_handle(), SIGUSR1); // interrupt the blocked ::read
	std::this_thread::sleep_for(200ms);
	REQUIRE(write(fds[1], "ping", 4) == 4);
	reader.join();

	CHECK_FALSE(threw);
	CHECK(bytes_read == 4);
	CHECK(std::memcmp(buf, "ping", 4) == 0);
	close(fds[1]);
}

TEST_CASE("fd errors carry errno via std::system_error", "[io][fd]")
{
	// write to the read end of a pipe: EBADF
	int fds[2];
	REQUIRE(pipe(fds) == 0);
	mh::io::fd_sink bad_sink(fds[0], true); // read end - writes must fail

	bool threw_ebadf = false;
	try
	{
		(void)bad_sink.write_async("x", 1).get();
	}
	catch (const std::system_error& e)
	{
		threw_ebadf = (e.code() == std::errc::bad_file_descriptor);
	}
	CHECK(threw_ebadf);
	close(fds[1]);
}

TEST_CASE("fd_sink/fd_source - close semantics", "[io][fd]")
{
	int fds[2];
	REQUIRE(pipe(fds) == 0);
	mh::io::fd_sink sink(fds[1], true);

	REQUIRE(sink.is_open());
	sink.close();
	CHECK_FALSE(sink.is_open());
	sink.close(); // second close must be harmless (handle stores invalid after reset)

	// the coroutine starts eagerly, so the failure is captured in the task
	CHECK_THROWS(sink.write_async("x", 1).get());
	close(fds[0]);
}

TEST_CASE("fd_sink - take_ownership=false duplicates the descriptor", "[io][fd]")
{
	int fds[2];
	REQUIRE(pipe(fds) == 0);

	{
		mh::io::fd_sink borrowing(fds[1], false);
		REQUIRE(borrowing.is_open());
		CHECK(borrowing.get_native_handle() != fds[1]); // dup()ed
		borrowing.close();
	}

	// the caller's original descriptor must still be usable
	CHECK(write(fds[1], "y", 1) == 1);
	close(fds[0]);
	close(fds[1]);
}

TEST_CASE("sink::create_file / source::create_file round trip", "[io][fd]")
{
	// regression: both factories were declared MH_STUFF_API but defined nowhere -
	// guaranteed undefined reference for any caller, in both library modes.
	const auto path = std::filesystem::temp_directory_path() / "mh_stuff_io_fd_test.txt";

	auto sink = mh::io::sink::create_file(path);
	REQUIRE(sink != nullptr);
	REQUIRE(sink->is_open());
	CHECK(sink->write_async("file-data", 9).get() == 9);
	sink->close();

	// append mode appends instead of truncating
	auto appender = mh::io::sink::create_file(path, true);
	CHECK(appender->write_async("+more", 5).get() == 5);
	appender->close();

	auto source = mh::io::source::create_file(path);
	REQUIRE(source != nullptr);
	char buf[32] = {};
	CHECK(source->read_async(buf, sizeof(buf)).get() == 14);
	CHECK(std::string(buf) == "file-data+more");
	source->close();

	std::filesystem::remove(path);

	// a missing file reports the real errno
	bool threw_enoent = false;
	try
	{
		(void)mh::io::source::create_file(path);
	}
	catch (const std::system_error& e)
	{
		threw_enoent = (e.code() == std::errc::no_such_file_or_directory);
	}
	CHECK(threw_enoent);
}

#endif // __unix__
