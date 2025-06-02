#ifdef MH_COMPILE_LIBRARY
#include "process.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef __unix__

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>

#include <mh/error/not_implemented_error.hpp>
#include <mh/io/fd_source.hpp>
#include <mh/io/fd_sink.hpp>
#include <mh/io/pipe.hpp>
#include <mh/io/native_handle.hpp>

#include "process_manager.hpp"

extern char **environ;

namespace mh
{

// Process implementation
struct process::impl
{
	std::string command_;
	std::vector<std::string> args_;
	io::source_ptr input_source_;
	io::sink_ptr output_sink_;
	io::sink_ptr error_sink_;
	int pid_ = 0;
	bool started_ = false;
	bool completed_ = false;
	int exit_code_ = 0;

	impl(const std::string& command, const std::vector<std::string>& args, io::source_ptr input_source,
	     io::sink_ptr output_sink, io::sink_ptr error_sink)
	    : command_(command), args_(args), input_source_(input_source), output_sink_(output_sink),
	      error_sink_(error_sink)
	{

		// Ensure command is first in args list
		if (args_.empty() || args_[0] != command_)
		{
			args_.insert(args_.begin(), command_);
		}
	}

	bool start()
	{
		if (started_)
			return false;

		// Set up file actions for posix_spawn
		posix_spawn_file_actions_t file_actions;
		if (posix_spawn_file_actions_init(&file_actions) != 0)
		{
			throw std::runtime_error("Failed to initialize posix_spawn file actions");
		}

		// Configure I/O redirections
		auto setup_redirection = [&](auto& io_ptr, int target_fd, const char* desc) {
			if (io_ptr)
			{
				io::native_handle fd = io_ptr->get_native_handle();
				if (fd < 0)
				{
					posix_spawn_file_actions_destroy(&file_actions);
					throw std::runtime_error(std::string("Invalid file descriptor for ") + desc);
				}
				if (posix_spawn_file_actions_adddup2(&file_actions, fd, target_fd) != 0)
				{
					posix_spawn_file_actions_destroy(&file_actions);
					throw std::runtime_error(std::string("Failed to configure ") + desc + " redirection");
				}
			}
		};
		
		setup_redirection(input_source_, STDIN_FILENO, "stdin");
		setup_redirection(output_sink_, STDOUT_FILENO, "stdout");
		setup_redirection(error_sink_, STDERR_FILENO, "stderr");

		// Convert args to C-style array
		char** arg_array = new char*[args_.size() + 1];
		for (size_t i = 0; i < args_.size(); ++i)
		{
			arg_array[i] = const_cast<char*>(args_[i].c_str());
		}
		arg_array[args_.size()] = nullptr;

		// Spawn the process
		int result = posix_spawnp(&pid_, command_.c_str(), &file_actions, nullptr, arg_array, environ);

		// Clean up
		delete[] arg_array;
		posix_spawn_file_actions_destroy(&file_actions);

		if (result == 0)
		{
			started_ = true;
			return true;
		}
		else
		{
			throw std::runtime_error("Failed to spawn process: " + command_);
		}
	}

	task<int> wait_async()
	{
		if (!started_)
		{
			co_return -1;
		}

		if (completed_)
		{
			co_return exit_code_;
		}

		// First check if process already exited
		int status;
		pid_t result = waitpid(pid_, &status, WNOHANG);
		if (result > 0)
		{
			completed_ = true;
			if (WIFEXITED(status))
			{
				exit_code_ = WEXITSTATUS(status);
			}
			else if (WIFSIGNALED(status))
			{
				exit_code_ = -WTERMSIG(status);
			}
			else
			{
				exit_code_ = -1;
			}
			co_return exit_code_;
		}
		else if (result == -1)
		{
			completed_ = true;
			exit_code_ = -1;
			co_return exit_code_;
		}

		// Register with process manager for async waiting
		struct process_awaiter
		{
			int pid;
			impl* process_impl;

			bool await_ready()
			{
				return false;
			}

			void await_suspend(std::coroutine_handle<> handle)
			{
				process_manager::instance().register_process(pid, handle);
			}

			int await_resume()
			{
				int exit_status = process_manager::instance().get_exit_status(pid);
				process_manager::instance().unregister_process(pid);

				process_impl->completed_ = true;
				process_impl->exit_code_ = exit_status;
				return exit_status;
			}
		};

		co_return co_await process_awaiter{pid_, this};
	}

	bool is_running() const
	{
		if (!started_ || completed_)
			return false;
		return kill(pid_, 0) == 0;
	}

	bool terminate(bool force)
	{
		if (!started_ || completed_)
			return false;
		return kill(pid_, force ? SIGKILL : SIGTERM) == 0;
	}

};

// Process public interface
MH_COMPILE_LIBRARY_INLINE process::process(const std::string& command, const std::vector<std::string>& args,
                                           io::source_ptr input_source, io::sink_ptr output_sink,
                                           io::sink_ptr error_sink)
    : m_impl(std::make_unique<impl>(command, args, input_source, output_sink, error_sink))
{}

MH_COMPILE_LIBRARY_INLINE process::~process() = default;

MH_COMPILE_LIBRARY_INLINE bool process::start()
{
	return m_impl->start();
}

MH_COMPILE_LIBRARY_INLINE task<int> process::wait_async()
{
	return m_impl->wait_async();
}

MH_COMPILE_LIBRARY_INLINE bool process::is_running() const
{
	return m_impl->is_running();
}

MH_COMPILE_LIBRARY_INLINE int process::get_pid() const
{
	return m_impl->pid_;
}

MH_COMPILE_LIBRARY_INLINE bool process::terminate(bool force)
{
	return m_impl->terminate(force);
}
} // namespace mh

#endif // __unix__
