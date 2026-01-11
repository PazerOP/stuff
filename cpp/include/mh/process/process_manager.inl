#ifdef MH_COMPILE_LIBRARY
#include "process_manager.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef __unix__

#include <mh/concurrency/dispatcher.hpp>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>

namespace mh
{
	MH_COMPILE_LIBRARY_INLINE process_manager& process_manager::instance()
	{
		static process_manager mgr;
		return mgr;
	}

	MH_COMPILE_LIBRARY_INLINE bool process_manager::register_process(int pid, std::coroutine_handle<> handle)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		waiting_processes_[pid] = {handle, -1};

		// Install signal handler and start monitoring if this is the first process
		if (waiting_processes_.size() == 1)
		{
			if (!signal_handler_installed_)
			{
				install_signal_handler();
			}
			if (!monitoring_started_)
			{
				start_monitoring_task();
				monitoring_started_ = true;
			}
		}

		return true;
	}

	MH_COMPILE_LIBRARY_INLINE void process_manager::unregister_process(int pid)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		waiting_processes_.erase(pid);
		exit_statuses_.erase(pid);
	}

	MH_COMPILE_LIBRARY_INLINE int process_manager::get_exit_status(int pid)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = exit_statuses_.find(pid);
		return (it != exit_statuses_.end()) ? it->second : -1;
	}

	MH_COMPILE_LIBRARY_INLINE process_manager::process_manager()
	{
		// Signal handler will be installed when first process registers
	}

	MH_COMPILE_LIBRARY_INLINE void process_manager::install_signal_handler()
	{
		if (!signal_handler_installed_)
		{
			// Create self-pipe for signal handling
			if (pipe(signal_pipe_) == 0)
			{
				// Set write end to non-blocking
				int flags = fcntl(signal_pipe_[1], F_GETFL);
				fcntl(signal_pipe_[1], F_SETFL, flags | O_NONBLOCK);

				// Install SIGCHLD handler
				struct sigaction sa;
				sa.sa_handler = signal_handler;
				sigemptyset(&sa.sa_mask);
				sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
				sigaction(SIGCHLD, &sa, nullptr);

				signal_handler_installed_ = true;
			}
		}
	}

	MH_COMPILE_LIBRARY_INLINE void process_manager::signal_handler(int)
	{
		char byte = 1;
		write(signal_pipe_[1], &byte, 1);
	}

	MH_COMPILE_LIBRARY_INLINE void process_manager::start_monitoring_task()
	{
		// Start a long-running task that monitors for SIGCHLD
		auto monitor = [this]() -> task<void>
		{
			while (true)
			{
				// Wait for SIGCHLD notification
				co_await dispatcher::get().co_wait_fd_read(signal_pipe_[0]);

				// Drain the pipe
				char buffer[256];
				read(signal_pipe_[0], buffer, sizeof(buffer));

				// Check all waiting processes
				check_processes();

				// Keep monitoring as long as there are processes
				{
					std::lock_guard<std::mutex> lock(mutex_);
					if (waiting_processes_.empty())
					{
						monitoring_started_ = false;
						co_return; // No more processes to monitor
					}
				}
			}
		};

		// Start the monitoring task (detached)
		(void)monitor();
	}

	MH_COMPILE_LIBRARY_INLINE void process_manager::check_processes()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		for (auto it = waiting_processes_.begin(); it != waiting_processes_.end();)
		{
			int pid = it->first;
			auto &info = it->second;

			int status;
			pid_t result = waitpid(pid, &status, WNOHANG);

			if (result > 0)
			{
				// Process completed
				int exit_code;
				if (WIFEXITED(status))
				{
					exit_code = WEXITSTATUS(status);
				}
				else if (WIFSIGNALED(status))
				{
					exit_code = -WTERMSIG(status);
				}
				else
				{
					exit_code = -1;
				}

				// Store exit status and resume coroutine
				exit_statuses_[pid] = exit_code;
				info.handle.resume();

				// Remove from waiting list
				it = waiting_processes_.erase(it);
			}
			else if (result == -1)
			{
				// Error occurred
				exit_statuses_[pid] = -1;
				info.handle.resume();
				it = waiting_processes_.erase(it);
			}
			else
			{
				// Process still running
				++it;
			}
		}
	}

	// Static member definitions - guard with MH_COMPILE_LIBRARY_INLINE to prevent multiple definitions
	MH_COMPILE_LIBRARY_INLINE bool process_manager::signal_handler_installed_ = false;
	MH_COMPILE_LIBRARY_INLINE int process_manager::signal_pipe_[2] = {-1, -1};
}

#endif // __unix__
