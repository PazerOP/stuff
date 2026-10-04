#ifdef MH_COMPILE_LIBRARY
#include "process.hpp"
#else
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

#ifdef __unix__

#include "process_manager.hpp"
#include <mh/error/not_implemented_error.hpp>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

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

        impl(const std::string &command, const std::vector<std::string> &args,
             io::source_ptr input_source, io::sink_ptr output_sink, io::sink_ptr error_sink)
            : command_(command), args_(args), input_source_(input_source),
              output_sink_(output_sink), error_sink_(error_sink)
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

            // io redirection is not implemented yet: fail cleanly in the parent,
            // never after fork. Throwing in the forked child would unwind a copy of
            // the parent's call stack - a caller that catches would keep the child
            // alive as a duplicate of the application.
            if (input_source_ || output_sink_ || error_sink_)
                throw mh::not_implemented_error(MH_SOURCE_LOCATION_CURRENT());

            // Build argv before fork: allocating after fork in a multithreaded
            // process can deadlock on an allocator lock held by a defunct thread.
            std::vector<char*> arg_array;
            arg_array.reserve(args_.size() + 1);
            for (auto& arg : args_)
                arg_array.push_back(arg.data());
            arg_array.push_back(nullptr);

            pid_ = fork();

            if (pid_ == -1)
            {
                return false; // Fork failed
            }
            else if (pid_ == 0)
            {
                // Child process: execute the command
                execvp(command_.c_str(), arg_array.data());

                // exec failed: _exit, not exit - running atexit handlers and
                // flushing stdio buffers duplicated from the parent would emit
                // buffered parent output twice. 127 mirrors the shell convention.
                _exit(127);
            }
            else
            {
                // Parent process
                started_ = true;
                return true;
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
                impl *process_impl;

                bool await_ready() { return false; }

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
    MH_COMPILE_LIBRARY_INLINE process::process(const std::string &command, const std::vector<std::string> &args,
                                               io::source_ptr input_source, io::sink_ptr output_sink, io::sink_ptr error_sink)
        : m_impl(std::make_unique<impl>(command, args, input_source, output_sink, error_sink))
    {
    }

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
}

#endif // __unix__
