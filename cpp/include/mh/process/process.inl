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

namespace mh
{

    // Process implementation
    struct process::impl
    {
        std::string command_;
        std::vector<std::string> args_;
        Source input_source_;
        Sink output_sink_;
        Sink error_sink_;
        int pid_ = 0;
        bool started_ = false;
        bool completed_ = false;
        int exit_code_ = 0;

        impl(const std::string &command, const std::vector<std::string> &args,
             Source input_source, Sink output_sink, Sink error_sink)
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

            pid_ = fork();

            if (pid_ == -1)
            {
                return false; // Fork failed
            }
            else if (pid_ == 0)
            {
                // Child process
                setup_child_io();

                // Convert args to C-style array
                char **arg_array = new char *[args_.size() + 1];
                for (size_t i = 0; i < args_.size(); ++i)
                {
                    arg_array[i] = const_cast<char *>(args_[i].c_str());
                }
                arg_array[args_.size()] = nullptr;

                // Execute the command
                execvp(command_.c_str(), arg_array);

                // If execvp returns, an error occurred
                delete[] arg_array;
                exit(1);
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

    private:
        void setup_child_io()
        {
            // Handle input (stdin)
            if (input_source_)
            {
                throw mh::not_implemented_error(); // Input redirection not yet implemented
            }

            // Handle output (stdout)
            if (output_sink_)
            {
                throw mh::not_implemented_error(); // Output redirection not yet implemented
            }

            // Handle error (stderr)
            if (error_sink_)
            {
                throw mh::not_implemented_error(); // Error redirection not yet implemented
            }
        }
    };

    // Process public interface
    MH_COMPILE_LIBRARY_INLINE process::process(const std::string &command, const std::vector<std::string> &args,
                                               Source input_source, Sink output_sink, Sink error_sink)
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
